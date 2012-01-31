#include <iostream>
#include <iomanip>

#include <nidas/util/Socket.h>
#include <nidas/core/Datagrams.h>
#include <nidas/core/SocketAddrs.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLFdInputSource.h>
#include <nidas/core/DatagramSocket.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::util;

struct request {
    unsigned int magic;
    int requestType;
    unsigned short listenPort;
    short socketType;
};

struct answer {
    unsigned int magic;
    short tcpPort;
    short udpPort;
    char strings[0];
};

struct tcpreq {
    unsigned int magic;
    unsigned short port;
};

/*
 * Connect to a Nidas UDP data feed.
 * if argv[1] is missing or multicast address (e.g. 239.0.0.10)
 * the connection will be a multicast feed.  Otherwise it will be unicast feed.
 */
void connect(int argc, char** argv)
{
    Inet4Address to;

    if (argc >= 2) to = Inet4Address::getByName(argv[1]);
    else to = Inet4Address::getByName(NIDAS_MULTICAST_ADDR);

    MulticastSocket dsock;
    dsock.bind(Inet4Address(INADDR_ANY),0);
    Socket tcpSock;

    if (to.isMultiCastAddress()) dsock.setInterface(to);

    short listenLocalPort = dsock.getLocalPort();

    request req;
    req.magic = htonl(0x01234567);
    req.requestType = htonl(nidas::core::UDP_PROCESSED_SAMPLE_FEED);
    req.listenPort = htons(listenLocalPort);
    req.socketType = htons(SOCK_DGRAM);

    DatagramPacketT<request> sendPkt(&req,sizeof(req));
    sendPkt.setSocketAddress(Inet4SocketAddress(to,NIDAS_SVC_REQUEST_PORT_UDP));

    size_t replen =sizeof(struct answer) + 1024;
    vector<char> repBuf(replen);
    struct answer* reply = (struct answer*) &repBuf.front();

    DatagramPacketT<struct answer> recvPkt(reply,replen);

    int udpPort;

    fd_set fdset;
    int nresp = 0;
    set<pair<Inet4SocketAddress,int> > servers;

    for (int i = 0; i < 3; i++ ) {
        dsock.send(sendPkt);

        int fd = dsock.getFd();
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        struct timeval tmpto = {1,0};
        int res;
        if ((res = ::select(fd+1,&fdset,0,0,&tmpto)) < 0) {
            throw IOException(dsock.getLocalSocketAddress().toString(),
                "select",errno);
        }
        if (res == 0) continue;
        if (res > 0) nresp++;
        dsock.receive(recvPkt);

        SocketAddress& saddr = recvPkt.getSocketAddress();

        cout << "received packet from " << saddr.toString() <<
            ", len=" << recvPkt.getLength() <<
            ", magic=" << ntohl(reply->magic) << 
            ", tcpPort=" << ntohs(reply->tcpPort) << 
            " udpPort=" << ntohs(reply->udpPort) << endl;
        const char* cp = reply->strings;
        const char* eop = &repBuf.front() + recvPkt.getLength();
        for ( ; cp < eop; ) {
            cout << "string=" << cp << endl;
            cp += strlen(cp) + 1;
        }
        if (saddr.getFamily() == AF_INET) {
            Inet4SocketAddress s4addr((const sockaddr_in*)saddr.getConstSockAddrPtr());
            s4addr.setPort(ntohs(reply->tcpPort));
            servers.insert(pair<Inet4SocketAddress,int>(s4addr,ntohs(reply->udpPort)));
        }
        sleep(1);
    }

    if (servers.size() == 0) {
        cerr << "No servers found" << endl;
        return;
    }

    int index = 0;

    set<pair<Inet4SocketAddress,int> >::const_iterator si;
    if (servers.size() > 1) {
        cout << "Enter a index for the desired address:" << endl;
        si = servers.begin();
        for (int i = 0; i < servers.size(); i++) {
            cout << i << ' ' << si->first.getInet4Address().getHostName() << endl;
            ++si;
        }
        cin >> index;
    }

    si = servers.begin();
    for (int i = 0; i < index; i++) ++si;
    pair<Inet4SocketAddress,int> server = *si;

    // const SocketAddress& saddr = recvPkt.getSocketAddress();
    Inet4SocketAddress s4addr = si->first;
    udpPort = si->second;
    cout << "selected addr=" << s4addr.toString() << endl;
    tcpSock.connect(s4addr);

    struct tcpreq tcpreq;
    tcpreq.magic = htonl(0x76543210);
    tcpreq.port = htons(listenLocalPort);
    tcpSock.send(&tcpreq,sizeof(tcpreq.magic) + sizeof(tcpreq.port));

// #define USE_XML_PARSER
#ifdef USE_XML_PARSER

    nidas::core::XMLParser parser;
    // throws XMLException

    // If parsing xml received from a server over a socket,
    // turn off validation - assume the server has validated the XML.
    parser.setDOMValidation(false);
    parser.setDOMValidateIfSchema(false);
    parser.setDOMNamespaces(true);
    parser.setXercesSchema(false);
    parser.setXercesSchemaFullChecking(false);
    parser.setDOMDatatypeNormalization(false);
    parser.setXercesUserAdoptsDOMDocument(true);

    xercesc::DOMDocument* doc = 0;
    try {
        std::string sockName = tcpSock.getRemoteSocketAddress().toString();
        nidas::core::XMLFdInputSource sockSource(sockName,tcpSock.getFd());
        doc = parser.parse(sockSource);
    }
    catch(const nidas::util::IOException& e) {
        ELOG(("requestXMLConfig:") << e.what());
        throw e;
    }
    catch(const nidas::core::XMLException& xe) {
        ELOG(("requestXMLConfig:") << xe.what());
        throw xe;
    }
    catch(...) {
        throw;
    }
#else

    char xmlbuf[32];
    bool eof = false;
    for ( ; !eof ; ) {
        size_t l = tcpSock.recv(xmlbuf,sizeof(xmlbuf));
        for (unsigned int i = 0; i < l; i++) {
            if (xmlbuf[i] == '\x04') {
                l = i;
                eof = true;
            }
        }
        // cerr << "l=" << l << " xml=" << string(xmlbuf,l) << endl;
        cerr << string(xmlbuf,l);
    }
    cerr << endl;
#endif

    if (to.isMultiCastAddress()) {
        dsock.close();
        dsock = MulticastSocket();
        dsock.bind(Inet4Address(INADDR_ANY),udpPort);
        dsock.joinGroup(to);
    }

#define PARSE_SAMPLES
#ifdef PARSE_SAMPLES
    nidas::dynld::SampleInputStream sis(new nidas::core::DatagramSocket(&dsock));
    sis.setExpectHeader(false);
    sis.setMaxSampleLength(32768);

    nidas::core::dsm_time_t lastTime = 0;
    for (;;) {
        nidas::core::Sample* samp = sis.readSample();
        nidas::core::dsm_time_t tt = samp->getTimeTag();
        float dt;
        if (lastTime != 0) dt = float(tt - lastTime) / USECS_PER_SEC;
        else dt = 0.0;
        lastTime = tt;
        cout << nidas::util::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%6f ") <<
            setprecision(5) << setw(8) <<  dt <<  ' ' << 
            GET_DSM_ID(samp->getId()) << ',' << GET_SPS_ID(samp->getId()) << ' ' <<
            samp->getDataByteLength();
        if (samp->getType() == nidas::core::FLOAT_ST) {
            const float* fptr = (const float* )samp->getConstVoidDataPtr();
            for (int i = 0; i < samp->getDataByteLength()/sizeof(float); i++)
                cout << ' ' << setprecision(5) << setw(8) << fptr[i];
        }
#ifdef DEBUG
        // const nidas::core::SampleHeader* hptr = (const SampleHeader*) samp->getHeaderPtr();
        const nidas::core::dsm_sample_id_t* hptr = (const nidas::core::dsm_sample_id_t*) samp->getHeaderPtr();
        cout << ' ' << GET_SAMPLE_TYPE(hptr[3]);
#endif
        cout << endl;
    }

#else

    for ( ;; ) {
        char buf[16384];
        size_t l = dsock.recv(buf,sizeof(buf));
        cerr << "read data! l=" << l << endl;
    }
#endif
        
    dsock.close();
    tcpSock.close();
}

int main(int argc, char** argv)
{
    connect(argc,argv);
}
