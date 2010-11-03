/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-03-31 21:02:24 -0600 (Tue, 31 Mar 2009) $

    $LastChangedRevision: 4553 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/apps/dsm.cc $
 ********************************************************************

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
*/

#include <deque>

#include <nidas/util/Socket.h>
#include <nidas/util/Thread.h>
#include <nidas/util/Logger.h>
#include <nidas/core/Socket.h>
#include <nidas/core/IOStream.h>

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

    const std::deque<n_u::DatagramPacket*>& getPackets() const { return _packets; }

    nidas::util::Cond& dataReady() { return _dataReady; }

    void loop() throw();

    const string& getHeader() const { return _header; }

    int getTCPPort() const { return _tcpport; }

    bool debug() const { return _debug; }

private:
    int _udpport;
    int _tcpport;
    int _packetsize;
    string _header;
    std::deque<n_u::DatagramPacket*> _packets;
    nidas::util::Cond _dataReady;
    bool _debug;
};

PacketReader::PacketReader(): _udpport(-1),_tcpport(-1),_packetsize(16384),_debug(false)
{
}

PacketReader::~PacketReader()
{
    while (_packets.size()) {
        n_u::DatagramPacket* pkt = _packets.back();
        delete [] pkt->getData();
        delete pkt;
        _packets.pop_back();
    }
}

int PacketReader::usage(const char* argv0)
{
    cerr << "\n\
Usage: " << argv0 << "-h header_file -u port -t port [-d]\n\
    -h header_file: the name of a file containing a NIDAS header: \"NIDAS (ncar.ucar.edu)...\"\n\
    -u port: UDP port to read from\n\
    -t port: TCP port to wait on for connections\n]\
    -d: debug, don't run in background" << endl;
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
        n_u::IOException e(argv[1],"open",errno);
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

void PacketReader::loop() throw()
{
    for (int i = 0; i < 2; i++)
    {
        char* buf = new char[_packetsize];
        n_u::DatagramPacket* pkt = new n_u::DatagramPacket(buf,_packetsize);
        _packets.push_front(pkt);
    }

    for (; !interrupted;) {

        try {
            n_u::DatagramSocket sock(_udpport);
            // cerr << "udpport=" << _udpport << endl;

            try {
                for (unsigned int n = 0; !interrupted; n++) {

                    _dataReady.lock();
                    n_u::DatagramPacket* pkt = _packets.back();
                    _packets.pop_back();
                    _dataReady.unlock();

                    sock.receive(*pkt);
#ifdef DEBUG
                    if (!(n % 100))
                        cerr << "received packet, length=" << pkt->getLength() << " from " <<
                            pkt->getSocketAddress().toString() << endl;
#endif

                    _dataReady.lock();
                    _packets.push_front(pkt);
                    _dataReady.broadcast();
                    _dataReady.unlock();
                }
            }
            catch(const n_u::IOException& e) {
                PLOG(("%s",e.what()));
                sleep(5);
            }
            sock.close();
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
};

WriterThread::WriterThread(n_u::Socket* sock,PacketReader& reader):
    n_u::DetachedThread("TCPWriter"),_reader(reader),_header(reader.getHeader()),_sock(sock)
{
}

int WriterThread::run() throw(n_u::Exception)
{
    try {
        nidas::core::Socket ncSock(_sock);
        nidas::core::IOStream ios(ncSock);

        ios.write(_header.c_str(),_header.length(),false);

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
    n_u::DetachedThread("TCPServer"),_reader(reader),_header(reader.getHeader()),_port(reader.getTCPPort())
{
}

int ServerThread::run() throw(n_u::Exception)
{
        try {
            n_u::ServerSocket ssock(_port);
            for ( ;! interrupted; ) {
                n_u::Socket* sock = ssock.accept();
                WriterThread* writer = new WriterThread(sock,_reader);
                writer->start();
            }
        }
        catch(const n_u::IOException& e) {
            PLOG(("%s",e.what()));
            interrupted = true;
        }
    return RUN_EXCEPTION;
}


int main(int argc, char** argv)
{

    PacketReader reader;
    int res = reader.parseRunstring(argc,argv);
    if (res) return res;

    n_u::Logger* logger;
    n_u::LogConfig lc;

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
        logger = n_u::Logger::createInstance("nidas_udp_relay",LOG_CONS,LOG_LOCAL5);
        lc.level = 5;
    }
    logger->setScheme(n_u::LogScheme("nidas_udp_relay").addConfig (lc));


    // detached thread. Will delete itself.
    ServerThread* server = new ServerThread(reader);
    server->start();

    reader.loop();
}
