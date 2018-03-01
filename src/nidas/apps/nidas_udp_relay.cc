/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*

 Program for relaying NIDAS data samples received on a UDP port to TCP
 clients.  This can help solve two problems in data distribution from a
 remote station:

 1. The network to the station may be variable, for example a cellular
    modem connection where maintaining a TCP connection can be difficult.
    Instead of TCP, stations send UDP data packets to a known data gateway
    running this relay.  All stations can send to the same port on the same
    host.

 2. Systems with a good network connection to the gateway can then
    establish a TCP connection to the gateway running this relay. Stateful
    firewalls may need to be setup to allow the TCP connection.
 
 Because it uses UDP, there is no guarantee of 100% data recovery, and the
 data packets may arrive out of order.  This is intended to be used for
 real-time, QC/quicklook purposes, and not for the data system archive.

 This program basically does no buffering of packets.  A read packet is
 placed in a deque and a Cond::broadcast is done to notify
 the writer threads that a packet is available to write.
*/

#include <deque>

#include <nidas/util/Process.h>
#include <nidas/util/Socket.h>
#include <nidas/util/Thread.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/core/Socket.h>
#include <nidas/core/IOStream.h>
#include <nidas/core/Sample.h>
#include <nidas/core/NidasApp.h>

#include <unistd.h>
#include <getopt.h>

using namespace std;

namespace n_u = nidas::util;
using nidas::core::NidasApp;
using nidas::core::NidasAppArgv;
using nidas::core::ArgVector;
using nidas::util::LogScheme;
using nidas::util::LogConfig;
using nidas::util::Logger;

static bool interrupted = false;

class PacketReader
{
public:
    PacketReader();
    ~PacketReader();

    int parseRunstring(int argc, char** argv);

    int usage(const char* argv0);

    const deque<n_u::DatagramPacket*>& getPackets() const { return _packets; }
    nidas::util::Cond& dataReady() { return _dataReady; }

    void loop() throw();

    const string& getHeader() const { return _header; }

    int getTCPPort() const { return _tcpport; }

    bool debug() const { return _debug; }

    int getMaxPacketSize() const { return _packetsize; }

    void checkPacket(n_u::DatagramPacket&);

    void logBadPacket(const n_u::DatagramPacket& pkt, const string& msg);

private:
    int _udpport;
    int _tcpport;
    int _packetsize;
    string _header;
    deque<n_u::DatagramPacket*> _packets;
    nidas::util::Cond _dataReady;
    bool _debug;
    static const int DEFAULT_PACKET_SIZE = 16384;

    size_t _rejectedPackets;
    long long _minSampleTime;
    long long _maxSampleTime;

    int _maxDsmId;

    unsigned int _maxSampleLength;
    int _rejectPacketInterval;
    int _packetReadInterval;

    NidasApp _app;
};

PacketReader::PacketReader():
    _udpport(-1),_tcpport(-1),
    _packetsize(DEFAULT_PACKET_SIZE),_header(),
    _packets(),_dataReady(),_debug(false),
    _rejectedPackets(0),
    _minSampleTime(n_u::UTime().toUsecs() - USECS_PER_SEC * 3600LL),
    _maxSampleTime(_minSampleTime + USECS_PER_DAY * 365LL),
    _maxDsmId(1000),_maxSampleLength(8192),
    _rejectPacketInterval(100),
    _packetReadInterval(100),
    _app("nidas_udp_relay")
{
    _app.setApplicationInstance();
}

PacketReader::~PacketReader()
{
    try {
        _dataReady.lock();
        while (!_packets.empty()) {
            n_u::DatagramPacket* pkt = _packets.back();
            delete [] pkt->getData();
            delete pkt;
            _packets.pop_back();
        }
        _dataReady.unlock();
    }
    catch(const n_u::Exception& e) {
        // shouldn't happen. _dataReady should be unlocked
        // when this destructor is called
        CLOG(("~PacketReader: ") << e.what());
        std::terminate();
    }
}

int PacketReader::usage(const char* argv0)
{
    cerr <<
        "\n"
        "Usage: " << argv0
         << " [-d] -h header_file [-p packetsize] -u port [-t port]\n"
        " -d   debug, don't run in background\n"
        " -h header_file:\n"
        "      the name of a file containing a NIDAS header:\n"
        "      beginning with \"NIDAS (ncar.ucar.edu)...\"\n"
        " -p packetsize:\n"
        "      max size in byte of the expected packets. \n"
        "      Default=" << DEFAULT_PACKET_SIZE << "\n"
        " -t port:\n"
        "      TCP port to wait on for connections.\n"
        "      Defaults to same as UDP port\n"
        " -u port:\n"
        "      UDP port on local interfaces to read from\n"
        "NIDAS options:\n"
         << _app.usage();
    return 1;
}

int PacketReader::parseRunstring(int argc, char** argv)
{
    _app.enableArguments(_app.loggingArgs() |
                         _app.Version | 
                         _app.Help);
    // conflicts with header file.
    _app.Help.acceptShortFlag(false);
    
    // The default logging scheme logs to syslog.  Set it here so it can be
    // overridden by command-line options.  Note things are a little broken
    // here because the NidasApp logging options are applied first, even if
    // they appear after the nidas_udp_relay -d option.  Not sure what can
    // be done about that until the new API is merged which allows
    // sequential option handling.
    Logger* logger = Logger::createInstance("nidas_udp_relay",
                                            LOG_PID, LOG_LOCAL5);
    LogScheme logscheme("syslog");
    logscheme.setShowFields("level,message");
    logscheme.addConfig(LogConfig("level=info"));
    logger->setScheme(logscheme);

    ArgVector args = _app.parseArgs(ArgVector(argv+1, argv+argc));
    if (_app.helpRequested())
    {
        return usage(argv[0]);
    }

    NidasAppArgv left(argv[0], args);
    extern char *optarg;       /* set by getopt() */
    // extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    const char* headerFileName = 0;
    char* cp;

    argc = left.argc;
    argv = left.argv;
    while ((opt_char = getopt(argc, argv, "dh:p:t:u:")) != -1) {
        switch(opt_char) {
        case 'd':
            _debug = true;
            logger = n_u::Logger::createInstance(&std::cerr);
            {
                LogScheme current = logger->getScheme();
                current.addConfig(LogConfig("level=debug"));
                logger->setScheme(current);
            }
            break;
        case 'h':
            headerFileName = optarg;
            break;
        case 'p':
            _packetsize = strtol(optarg,&cp,10);
            if (cp == optarg) return usage(argv[0]);
            break;
        case 't':
            _tcpport = strtol(optarg,&cp,10);
            if (cp == optarg) return usage(argv[0]);
            break;
        case 'u':
            _udpport = strtol(optarg,&cp,10);
            if (cp == optarg) return usage(argv[0]);
            break;
        }
    }
    if (headerFileName == 0) return usage(argv[0]);
    if (_udpport < 0) return usage(argv[0]);

    if (_tcpport < 0) _tcpport = _udpport;

    _header.clear();
    FILE* fp = fopen(headerFileName,"r");
    if (! fp) {
        n_u::IOException e(headerFileName,"open",errno);
        cerr << e.what() << endl;
        return usage(argv[0]);
    }

    for (; !feof(fp) && !ferror(fp); ) {
        char buf[64];
        if (fgets(buf,sizeof(buf),fp)) _header += buf;
    }
    if (ferror(fp)) {
        n_u::IOException e(argv[1],"open",errno);
        cerr << e.what() << endl;
        return usage(argv[0]);
    }
    fclose(fp);

    _packetReadInterval = 
        LogScheme::current().getParameterT("udp_relay_packet_interval",
                                           _packetReadInterval);
    _rejectPacketInterval = 
        LogScheme::current().getParameterT("udp_relay_reject_interval",
                                           _rejectPacketInterval);
    return 0;
}

void PacketReader::logBadPacket(const n_u::DatagramPacket& pkt, const string& msg)
{
    char outstr[32],*outp = outstr;
    const char* cp = (const char*) pkt.getConstDataVoidPtr();
    for (int i = 0; i < 8 && i < pkt.getLength(); i++,cp++) {
        outp += sprintf(outp,"%02x",(unsigned int)*cp);
    }
    *outp = '\0';

    WLOG(("bad packet #") << _rejectedPackets <<
        " from " << pkt.getSocketAddress().toString() <<
        ": " << msg << ", initial 8 bytes in hex: " << outstr);
}

void PacketReader::checkPacket(n_u::DatagramPacket& pkt)
{
    const char* sod = (const char*) pkt.getConstDataVoidPtr();
    const char* eod = sod + pkt.getLength();
    const char* dptr = sod;
    nidas::core::SampleHeader header;

    // The following header checks assume host is little endian
    assert(__BYTE_ORDER == __LITTLE_ENDIAN);

    for (; dptr < eod;) {

        if (dptr + (signed) nidas::core::SampleHeader::getSizeOf() > eod) {
            if (!(_rejectedPackets++ % _rejectPacketInterval)) {
                ostringstream ost;
                ost << "short header starting at byte "
                    << (size_t)(dptr - sod)
                    << ", total packet length=" << pkt.getLength() << " bytes";
                logBadPacket(pkt,ost.str());
            }
            break;
        }

        memcpy(&header,dptr,nidas::core::SampleHeader::getSizeOf()); 
        const char* eos = dptr + nidas::core::SampleHeader::getSizeOf() +
            header.getDataByteLength();

        if (header.getType() != nidas::core::CHAR_ST ||
            (signed) GET_DSM_ID(header.getId()) > _maxDsmId ||
            eos > eod ||
            header.getDataByteLength() > _maxSampleLength ||
            header.getDataByteLength() == 0 ||
            header.getTimeTag() < _minSampleTime ||
            header.getTimeTag() > _maxSampleTime) {
            if (!(_rejectedPackets++ % _rejectPacketInterval)) {
                ostringstream ost;
                ost << "bad header: type=0x" << hex << (int)header.getType() << dec <<
                    ", id=" << GET_DSM_ID(header.getId()) << ',' << GET_SPS_ID(header.getId()) <<
                    ", len=" << header.getDataByteLength() << ",ttag=" <<
                    n_u::UTime(header.getTimeTag()).format(true,"%Y %m %d %H:%M:%S");
                logBadPacket(pkt,ost.str());
            }
            break;
        }
        dptr = eos;
    }
    pkt.setLength((size_t)(dptr - sod));
}

void PacketReader::loop() throw()
{
    for (int i = 0; i < 2; i++)
    {
        char* buf = new char[_packetsize];
        n_u::DatagramPacket* pkt = new n_u::DatagramPacket(buf,_packetsize);
        _packets.push_front(pkt);
    }

    for (; !interrupted; ) {

        try {
            n_u::DatagramSocket sock(_udpport);

            try {
                _dataReady.lock();
                for (unsigned int n = 0; !interrupted; n++)
                {
                    n_u::DatagramPacket* pkt = _packets.back();
                    _packets.pop_back();
                    _dataReady.unlock();

                    if (!(n % _packetReadInterval))
                    {
                        VLOG(("calling receive() with max packet size ")
                             << pkt->getMaxLength());
                    }
                    sock.receive(*pkt);

                    // screen for non NIDAS packets
                    checkPacket(*pkt);
                    if (!(n % _packetReadInterval))
                    {
                        VLOG(("received packet, length=") << pkt->getLength()
                             << " from "
                             << pkt->getSocketAddress().toString());
                    }
                    _dataReady.lock();
                    _packets.push_front(pkt);
                    _dataReady.broadcast();
                }
                _dataReady.unlock();
            }
            catch(const n_u::IOException& e) {
                // _dataReady will be unlocked
                PLOG(("%s",e.what()));
                sleep(5);
            }
            sock.close();
            // try to re-open, until interrupted
        }
        catch(const n_u::IOException& e) {
            PLOG(("%s",e.what()));
            sleep(5);
        }
    }
}

class WriterThread: public n_u::DetachedThread
{
public:
    WriterThread(n_u::Socket* sock,PacketReader& reader);
    int run() throw(n_u::Exception);
private:
    PacketReader& _reader;
    string _header;
    nidas::core::Socket _ncSock;
    int _packetWriterInterval;

    /** No copying. */
    WriterThread(const WriterThread&);
    /** No assignment. */
    WriterThread& operator=(const WriterThread&);
};

WriterThread::WriterThread(n_u::Socket* sock,PacketReader& reader):
    n_u::DetachedThread("TCPWriter"),
    _reader(reader),
    _header(reader.getHeader()),
    _ncSock(sock),
    _packetWriterInterval(100)
{
    _packetWriterInterval = 
        LogScheme::current().getParameterT("udp_relay_write_interval",
                                           _packetWriterInterval);

}

int WriterThread::run() throw(n_u::Exception)
{
    try {

        // set to non blocking since we hold a lock during the write
        _ncSock.setNonBlocking(true);

        nidas::core::IOStream ios(_ncSock, _reader.getMaxPacketSize());

        ios.write(_header.c_str(), _header.length(), true);

        _reader.dataReady().lock(); // lock before first wait()

        for (unsigned int n = 0; !interrupted; n++) {
            _reader.dataReady().wait();
            if (_reader.getPackets().size() > 0) {
                n_u::DatagramPacket* pkt = _reader.getPackets().back();
                if (pkt->getLength() > 0)
                {
                    if (!(n % _packetWriterInterval))
                    {
                        VLOG(("writing packet, length=") << pkt->getLength()
                             << " to "
                             << _ncSock.getRemoteSocketAddress().toString());
                    }
                    // lock is held while writing, but flush every packet
                    // anyway so we don't introduce arbitrary latency
                    ios.write(pkt->getData(), pkt->getLength(), true);
                }
            }
        }
        _reader.dataReady().unlock();
    }
    catch(const n_u::IOException& e) {
        _reader.dataReady().unlock();
        if (e.getErrno() == EPIPE)  // normal shutdown
            ILOG(("%s",e.what()));
        else
            WLOG(("%s",e.what()));
        _ncSock.close();
    }
    return RUN_OK;
}

class ServerThread: public n_u::DetachedThread
{
public:
    ServerThread(PacketReader& reader);
    int run() throw(n_u::Exception);
private:
    PacketReader& _reader;
    string _header;
    int _port;
};

ServerThread::ServerThread(PacketReader& reader):
    n_u::DetachedThread("TCPServer"),_reader(reader),
    _header(reader.getHeader()),_port(reader.getTCPPort())
{
}

static bool serverSocketRetry(int err)
{
    /* from "man 2 accept":
     * For reliable operation the application should detect the
     * network errors defined for the protocol after accept() and treat them like  EAGAIN
     * by  retrying.   In  case of TCP/IP these are ENETDOWN, EPROTO, ENOPROTOOPT, EHOST-
     * DOWN, ENONET, EHOSTUNREACH, EOPNOTSUPP, and ENETUNREACH.
     */
    switch (err) {
    case ENETDOWN:
    case EPROTO:
    case ENOPROTOOPT:
    case EHOSTDOWN:
    case ENONET:
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
        return true;
    default:
        return false;
    }
}

int ServerThread::run() throw(n_u::Exception)
{
    try {
        for ( ; !isInterrupted(); ) {
            n_u::ServerSocket ssock(_port);
            for ( ; !isInterrupted(); ) {
                n_u::Socket* sock;
                try {
                    sock = ssock.accept();
                    sock->setKeepAliveIdleSecs(60);
                    ILOG(("New TCP Connection: ") <<
                        sock->getRemoteSocketAddress().toString());
                }
                catch(const n_u::IOException& e) {
                    ssock.close();
                    if (serverSocketRetry(e.getErrno())) {
                        WLOG(("%s", e.what()));
                        sleep(5);
                        continue;
                    }
                    throw;
                }
                // Detached thread deletes itself.
                WriterThread* writer = new WriterThread(sock, _reader);
                writer->start();
            }
            // interrupted
            ssock.close();
        }
    }
    catch(const n_u::IOException& e) {
        PLOG(("%s", e.what()));
    }
    interrupted = true;

    return RUN_EXCEPTION;
}


int main(int argc, char** argv)
{
    PacketReader reader;
    int res = reader.parseRunstring(argc,argv);
    if (res) return res;

    if (!reader.debug())
    {
        // fork to background
        if (daemon(0,0) < 0) {
            n_u::IOException e("nidas_udp_relay", "daemon", errno);
            cerr << "Warning: " << e.toString() << endl;
        }
    }
    NLOG(("nidas_udp_relay starting"));

#ifdef CAP_SYS_NICE
    try {
        n_u::Process::addEffectiveCapability(CAP_SYS_NICE);
    }
    catch (const n_u::Exception& e) {
        NLOG(("%s: %s. Will not be able to change process priority",
              argv[0], e.what()));
    }
#endif

    // Lower process nice value a little
    if (nice(-10) == -1) {
        ostringstream ost;
        ost << argv[0] << ": nice";
        n_u::Exception e(ost.str(),errno);
        NLOG(("%s, continuing anyway",e.what()));
    }

    // detached thread. Will delete itself.
    ServerThread* server = new ServerThread(reader);
    server->start();

    reader.loop();
}
