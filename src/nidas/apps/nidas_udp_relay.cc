/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
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

 Program for relaying NIDAS data samples received on a UDP port to TCP clients.
 This can help solve two problems in data distribution from a remote station:

 1. The network to the station may be variable, for example a cellular modem
    connection where maintaining a TCP connection can be difficult.  Instead of TCP,
    stations send UDP data packets to a known data gateway running this relay.
    All stations can send to the same port on the same host.
 2. Systems with a good network connection to the gateway can then establish a TCP
    connection to the gateway running this relay. Stateful firewalls may need
    to be setup to allow the TCP connection.
 
 Because it uses UDP, there is no guarantee of 100% data recovery, and the data packets
 may arrive out of order.  This is intended to be used for real-time, QC/quicklook
 purposes, and not for the data system archive.

 This program basically does no buffering of packets.  A read packet is
 placed in a deque and a Cond::broadcast is done to notify
 the writer threads that a packet is available to write. This may need
 to be improved.
*/

#include <deque>

#include <nidas/util/Socket.h>
#include <nidas/util/Thread.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/core/Socket.h>
#include <nidas/core/IOStream.h>
#include <nidas/core/Sample.h>

#include <unistd.h>
#include <getopt.h>

using namespace std;

namespace n_u = nidas::util;

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
};

PacketReader::PacketReader(): _udpport(-1),_tcpport(-1),
    _packetsize(DEFAULT_PACKET_SIZE),_header(),
    _packets(),_dataReady(),_debug(false),
    _rejectedPackets(0),
    _minSampleTime(n_u::UTime().toUsecs() - USECS_PER_SEC * 3600LL),
    _maxSampleTime(_minSampleTime + USECS_PER_DAY * 365LL),
    _maxDsmId(1000),_maxSampleLength(8192)
{
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
    cerr << "\n\
Usage: " << argv0 << "[-d] -h header_file [-p packetsize] -u port [-t port]\n\
    -d: debug, don't run in background\n\
    -h header_file: the name of a file containing a NIDAS header: \"NIDAS (ncar.ucar.edu)...\"\n\
    -p packetsize: max size in byte of the expected packets. Default=" << DEFAULT_PACKET_SIZE << "\n\
    -t port: TCP port to wait on for connections. Defaults to same as UDP port\n\
    -u port: UDP port on local interfaces to read from" << endl;
    return 1;
}

int PacketReader::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    // extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    const char* headerFileName = 0;
    char* cp;

    while ((opt_char = getopt(argc, argv, "dh:p:t:u:")) != -1) {
        switch(opt_char) {
        case 'd':
            _debug = true;
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
    return 0;
}

void PacketReader::logBadPacket(const n_u::DatagramPacket& pkt, const string& msg)
{
    char outstr[64],*outp = outstr;
    const char* cp = (const char*) pkt.getConstDataVoidPtr();
    for (int i = 0; i < 8 && i < pkt.getLength(); cp++) {
        if (isprint(*cp)) *outp++ = *cp;
        else outp += sprintf(outp,"%#02x",(unsigned int)*cp);
    }
    *outp = '\0';

    WLOG(("bad packet #%zd from %s: %s, initial 8 bytes: %s",
                _rejectedPackets,pkt.getSocketAddress().toString().c_str(),msg.c_str(),outstr));
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
            if (!(_rejectedPackets++ % 100)) {
                ostringstream ost;
                ost << "short header starting at byte " << (size_t)(dptr - sod) << ", total packet length=" << pkt.getLength() << " bytes";
                logBadPacket(pkt,ost.str());
            }
            break;
        }

        memcpy(&header,dptr,nidas::core::SampleHeader::getSizeOf()); 
        const char* eos = dptr + nidas::core::SampleHeader::getSizeOf() + header.getDataByteLength();

        if (header.getType() != nidas::core::CHAR_ST ||
            (signed) GET_DSM_ID(header.getId()) > _maxDsmId ||
            eos > eod ||
            header.getDataByteLength() > _maxSampleLength ||
            header.getDataByteLength() == 0 ||
            header.getTimeTag() < _minSampleTime ||
            header.getTimeTag() > _maxSampleTime) {
            if (!(_rejectedPackets++ % 100)) {
                ostringstream ost;
                ost << "bad header: type=" << header.getType() << ", id=" <<
                    GET_DSM_ID(header.getId()) << ',' << GET_SPS_ID(header.getId()) <<
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
		for (unsigned int n = 0; !interrupted; n++) {

		    n_u::DatagramPacket* pkt = _packets.back();
                    _packets.pop_back();
		    _dataReady.unlock();

		    sock.receive(*pkt);

		    // screen for non NIDAS packets
		    checkPacket(*pkt);
#ifdef DEBUG
		    if (!(n % 100))
			cerr << "received packet, length=" << pkt->getLength() << " from " <<
			    pkt->getSocketAddress().toString() << endl;
#endif
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
    n_u::Socket* _sock;
    /** No copying. */
    WriterThread(const WriterThread&);
    /** No assignment. */
    WriterThread& operator=(const WriterThread&);
};

WriterThread::WriterThread(n_u::Socket* sock,PacketReader& reader):
    n_u::DetachedThread("TCPWriter"),_reader(reader),
    _header(reader.getHeader()),_sock(sock)
{
}

int WriterThread::run() throw(n_u::Exception)
{
    try {

        // set to non blocking since we hold a lock during the write
        _sock->setNonBlocking(true);
        nidas::core::Socket ncSock(_sock);
        nidas::core::IOStream ios(ncSock,_reader.getMaxPacketSize());

        ios.write(_header.c_str(),_header.length(),false);

        _reader.dataReady().lock(); // lock before first wait()

        for (unsigned int n = 0; !interrupted; n++) {
            _reader.dataReady().wait();
            if (_reader.getPackets().size() > 0) {
                n_u::DatagramPacket* pkt = _reader.getPackets().back();
                if (pkt->getLength() > 0) {
#ifdef DEBUG
                    if (!(n % 100))
                        cerr << "writing packet, length=" << pkt->getLength() << " to " <<
                            _sock->getRemoteSocketAddress().toString() << endl;
#endif
                    // lock is held while writing... We'll try non-blocking writes
                    ios.write(pkt->getData(),pkt->getLength(),false);
                }
            }
        }
        _reader.dataReady().unlock();
    }
    catch(const n_u::IOException& e) {
        _reader.dataReady().unlock();
        ILOG(("%s",e.what()));
        _sock->close();
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
    unblockSignal(SIGTERM);
    unblockSignal(SIGINT);
    unblockSignal(SIGHUP);
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
                WriterThread* writer = new WriterThread(sock,_reader);
                try {
                    writer->setRealTimeRoundRobinPriority(50);
                }
                catch (const n_u::Exception& e) {
                    NLOG(("%s. Will continue without RT priority", e.what()));
                }
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

    n_u::Logger* logger;

    n_u::LogConfig lc;
    n_u::LogScheme logscheme("nidas_udp_relay");

    if (reader.debug()) {
        logger = n_u::Logger::createInstance(&std::cerr);
        lc.level = 7;
    }
    else {
        // fork to background
        if (daemon(0,0) < 0) {
            n_u::IOException e("nidas_udp_relay","daemon",errno);
            cerr << "Warning: " << e.toString() << endl;
        }
        logger = n_u::Logger::createInstance("nidas_udp_relay",LOG_PID,LOG_LOCAL5);
        logscheme.setShowFields("level,message");
        lc.level = 5;
    }
    logscheme.addConfig(lc);
    logger->setScheme(logscheme);
    NLOG(("nidas_udp_relay starting"));

    // block these signals in the main thread. They will be
    // caught by the ServerThread
    sigset_t signals;
    sigaddset(&signals,SIGTERM);
    sigaddset(&signals,SIGHUP);
    sigaddset(&signals,SIGINT);
    sigprocmask(SIG_BLOCK,&signals,NULL);

    // detached thread. Will delete itself.
    ServerThread* server = new ServerThread(reader);
    server->start();

    reader.loop();
}
