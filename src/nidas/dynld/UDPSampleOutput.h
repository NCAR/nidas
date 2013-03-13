// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved
 
    $LastChangedDate$
 
    $LastChangedRevision$
 
    $LastChangedBy$
 
    $HeadURL$
 ********************************************************************
 */

#ifndef NIDAS_DYNLD_UDPSAMPLEOUTPUT_H
#define NIDAS_DYNLD_UDPSAMPLEOUTPUT_H

#include <nidas/core/SampleOutput.h>
#include <nidas/core/ConnectionInfo.h>
#include <nidas/util/Thread.h>

#include <poll.h>

namespace nidas {

namespace util {
class Socket;
class ServerSocket;
}

namespace core {
class IOChannel;
class MultipleUDPSockets;
}

namespace dynld {
/**
 * Interface of an output stream of samples.
 */
class UDPSampleOutput: public nidas::core::SampleOutputBase
{
public:

    UDPSampleOutput();

    ~UDPSampleOutput();

    /**
     * Implementation of SampleClient::flush().
     */
    void flush() throw() {}

    void allocateBuffer(size_t len);

    nidas::core::SampleOutput* connected(nidas::core::IOChannel*) throw();

    bool receive(const nidas::core::Sample *s) throw();

    size_t write(const struct iovec* iov,int iovcnt) throw (nidas::util::IOException);

    // void init() throw();

    void close() throw(nidas::util::IOException);

    /**
     * Total number of bytes written with this IOStream.
     */
    long long getNumOutputBytes() const { return _nbytesOut; }

    void addNumOutputBytes(int val) { _nbytesOut += val; }

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

protected:

    /**
     * This SampleOutput does not support cloning.
     * It will die with an assert.
     */
    UDPSampleOutput* clone(nidas::core::IOChannel* iochannel);

    /**
     * This SampleOutput does not support a copy constructor with
     * a new IOChannel.  It will die with an assert.
     */
    UDPSampleOutput(UDPSampleOutput&,nidas::core::IOChannel*);

private:
    /**
     * Get a pointer to the current project DOM. The caller
     * acquires a read lock on the DOM, and must call releaseProjectDOM()
     * when they are finised.
     */
    xercesc::DOMDocument* getProjectDOM() throw(xercesc::DOMException);

    void releaseProjectDOM();

   /**
    * Worker thread that is run when a connection comes in,
    * sending XML over a socket.
    */
    class VariableListWorker: public nidas::util::Thread
    {
    public:
        /**
         * Constructor.
         */
        VariableListWorker(UDPSampleOutput* output,
            nidas::util::Socket* sock,bool keepOpen);
        ~VariableListWorker();
        int run() throw(nidas::util::Exception);
        void interrupt();
    private:
        UDPSampleOutput* _output;
        nidas::util::Socket* _sock;
        bool _keepOpen;
        /** No copying. */
        VariableListWorker(const VariableListWorker&);
        /** No assignment. */
        VariableListWorker& operator=(const VariableListWorker&);
    };

   /**
    * Thread that waits for connections to die.
    */
    class ConnectionMonitor: public nidas::util::Thread
    {
    public:
        ConnectionMonitor(nidas::core::MultipleUDPSockets* msock);
        ~ConnectionMonitor();
        int run() throw(nidas::util::Exception);
        void addConnection(nidas::util::Socket*,unsigned short udpport);
        void removeConnection(nidas::util::Socket*,unsigned short udpport);
        void addDestination(const nidas::core::ConnectionInfo&);
    private:
        void updatePollfds();
        nidas::core::MultipleUDPSockets* _msock;
        std::list<std::pair<nidas::util::Socket*,unsigned short> > _pendingSockets;
        std::list<std::pair<nidas::util::Socket*,unsigned short> > _pendingRemoveSockets;
        std::vector<std::pair<nidas::util::Socket*,unsigned short> > _sockets;
        std::map<nidas::util::Inet4SocketAddress,nidas::core::ConnectionInfo> _destinations;
        bool _changed;
        nidas::util::Mutex _sockLock;
        struct pollfd* _fds;
        int _nfds;
        /** No copying. */
        ConnectionMonitor(const ConnectionMonitor&);
        /** No assignment. */
        ConnectionMonitor& operator=(const ConnectionMonitor&);
    };

   /**
    * Thread that waits for a connection on a tcp socket,
    * starting a VariableListWorker on each connection.
    */
    class XMLSocketListener: public nidas::util::Thread
    {
    public:
        XMLSocketListener(UDPSampleOutput* output,
            int xmlPortNumber,ConnectionMonitor* monitor);
        ~XMLSocketListener();
        int run() throw(nidas::util::Exception);
        void interrupt();
    private:
        void checkWorkers() throw();
        void fireWorkers() throw();
        UDPSampleOutput* _output;
        nidas::util::ServerSocket* _sock;
        ConnectionMonitor* _monitor;
        std::list<VariableListWorker*> _workers;
        int _xmlPortNumber;
        /** No copying. */
        XMLSocketListener(const XMLSocketListener&);
        /** No assignment. */
        XMLSocketListener& operator=(const XMLSocketListener&);
    };

    nidas::core::MultipleUDPSockets* _mochan;

    xercesc::DOMDocument* _doc;

    bool _projectChanged;

    nidas::util::Mutex _docLock;

    nidas::util::RWLock _docRWLock;

    nidas::util::Mutex _listenerLock;

    unsigned short _xmlPortNumber;

    unsigned short _dataPortNumber;

    XMLSocketListener* _listener;

    ConnectionMonitor* _monitor;

    long long _nbytesOut;

    /** data buffer */
    char *_buffer;

    /** where we insert bytes into the buffer */
    char* _head;

    /** where we remove bytes from the buffer */
    char* _tail;

    /**
     * The actual buffer size.
     */
    size_t _buflen;

    /**
     * One past end of buffer.
     */
    char* _eob;

    /**
     * Time of last physical write.
     */
    nidas::core::dsm_time_t _lastWrite;

    /**
     * Maximum number of microseconds between physical writes.
     */
    int _maxUsecs;

private:

    /** No copying. */
    UDPSampleOutput(const UDPSampleOutput&);

    /** No assignment. */
    UDPSampleOutput& operator=(const UDPSampleOutput&);
};

/**
 * Structure sent back to client from the UDP feed server,
 * in big-endian order, indicating what TCP port number the
 * client should use to contact the server for the XML feed
 * of variables, and what port the binary data will be
 * multicast to.  The initial 4 bytes should be the MAGIC
 * value (big-endian 0x76543210) just in case some other
 * protocol is using the port.
 */
struct InitialUDPDataRequestReply
{
    unsigned int magic;      // should be MAGIC
    unsigned short int xmlTcpPort;
    unsigned short int dataMulticastPort;

    // a concatenated array of null terminated strings
    // 1. multicast address to listen on for data: e.g. "239.0.0.10"
    // 2. hostname which is providing UDP data feed.
    // 3-N: names of DSM where data was sampled.
    char strings[0];

    static const unsigned int MAGIC;
};


/**
 * Structure which the client must send back to server on the TCP 
 * port. It is a bit of a kludge, but the client must send
 * its local UDP port number back to the server, in big-endian
 * order, over the TCP connection in order for the server
 * to associate the TCP and UDP connections.
 * The initial 4 bytes should be the same MAGIC as in the
 * InitialUDPDataRequestReply.
 */
struct TCPClientResponse
{
    unsigned int magic;      // should be InitialUDPDataRequestReply::MAGIC
    unsigned short int clientUdpPort;
};

}}

#endif
