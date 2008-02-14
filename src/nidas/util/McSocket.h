/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_UTIL_MCSOCKET_H
#define NIDAS_UTIL_MCSOCKET_H

#include <nidas/util/Socket.h>
#include <nidas/util/DatagramPacket.h>
#include <nidas/util/Thread.h>

#include <list>

namespace nidas { namespace util {

struct McSocketData {

    /**
     * Magic value that should be found at the beginning of all received datagrams.
     */
    int magic;
    /**
     * Pseudo port number of request which identifies the request.
     * A McSocketListener on the server side must be doing
     * an accept for a McSocket with the same
     * requestNum value. Stored in "network", big-endian order.
     */
    int requestNum;

    /**
     * TCP stream socket port that the remote host is listening on.
     * Stored in "network", big-endian order.
     */
    unsigned short listenPort;

    /**
     * How many multicasts has it sent.
     * Stored in "network", big-endian order.
     */
    int numMulticasts;

    /**
     * Constructor.
     */
    McSocketData() : requestNum(0),listenPort(0),numMulticasts(0) {}
};

/**
 * Datagram that is multicast by a host when it wants a service.
 */
class McSocketDatagram: public DatagramPacketT<McSocketData>
{
public:
    McSocketDatagram(int requestNum=0);

    /**
     * Copy constructor.
     */
    McSocketDatagram(const McSocketDatagram& x);

    /**
     * Assignment operator.
     */ 
    McSocketDatagram& operator=(const McSocketDatagram& rhs);

    int getMagic() const { return ntohl(mcdata.magic); }
    void setMagic(int val) { mcdata.magic = htonl(val); }


    int getRequestNumber() const { return ntohl(mcdata.requestNum); }
    void setRequestNumber(int val) { mcdata.requestNum = htonl(val); }

    /**
     * What TCP port is the requester listening on for the connection back?
     */
    int getRequesterListenPort() const { return ntohs(mcdata.listenPort); }

    void setRequesterListenPort(int val) { mcdata.listenPort = htons(val); }

    /**
     * How patiently has the client been waiting?
     */
    int getNumMulticasts() const { return ntohl(mcdata.numMulticasts); }
    void setNumMulticasts(int val) { mcdata.numMulticasts = htonl(val); }
    
    /**
     * Magic value that should be found at the beginning of
     * all received McSocketDatagrams.
     */
    static const int magicVal;

protected:
    struct McSocketData mcdata;
};


/**
 * A McSocket provides a way to establish a TCP stream socket
 * using a Multicast protocol.
 * A McSocket can either listen for a connection (like a ServerSocket)
 * or connect to a remote McSocket, like a client.
 *
 * When a McSocket does an accept(), it registers itself with
 * a McSocketListener.  A McSocketListener is a thread which is
 * listening for McSocketDatagrams on a multicast address.
 *
 * When a McSocketDatagram is received, it contains the
 * address of the sending host, a stream socket port number
 * that the sending host is listening on, and a pseudo port number.
 * The McSocketListener checks if a McSocket has requested
 * a connection with the given pseudo port number.
 * If so, then it creates a Socket and connects it
 * to the stream socket port on the requesting host. This Socket
 * is returned as the value of the accept() method.
 *
 * When McSocket does a connect(), it starts a McSocketMulticaster
 * thread. This thread opens a ServerSocket using any available
 * port number. Then it multicasts McSocketDatagrams containing
 * the requestNum number of the McSocket, and the port number
 * of the ServerSocket. When a remote host has connected to
 * the ServerSocket, the thread quits, passing the accepted
 * socket back to the McSocket via the offer() method.
 */

class McSocket
{
    /**
     * McSocketListener and McSocketMulticaster are friends that setup
     * the socket connection and call the non-public offer() method.
     */
    friend class McSocketListener;
    friend class McSocketMulticaster;
public:
    /**
     * Create a McSocket, accepting multicast
     * requests for a socket connection.
     * Typical usage:
     *
*      @code
*      Inet4Address mcastAddr = Inet4Address::getByName("239.0.0.10");
*      int mport = 10000;
*      Inet4SocketAddress mcastSockAddr(mcastAddr,mport);
*      
*      int pport = 99;
*      
*      McSocket server;
*      server.setInet4McastSocketAddress(mcastSockAddr);
*      server.setRequestNumber(pport);
*      
*      McSocket client;
*      client.setInet4McastSocketAddress(mcastSockAddr);
*      client.setRequestNumber(pport);
*      
*      class McThread: public Thread {
*      public:
*         McThread(McSocket& clnt):
*	  	Thread("McThread"),client(clnt) {}
*	int run() throw(Exception)
*	{
*           Socket* socket = client.connect();
*	    char buf[16];
*	    size_t l = socket->recv(buf,sizeof(buf));
*	    buf[l] = '\0';
*	    cerr << "requester read, l=" << l << endl;
*	    if (strcmp(buf,"hello\n"))
*	        throw Exception("McThread socket read not as expected");
*	    try {
*		l = socket->recv(buf,sizeof(buf));
*	    }
*	    catch(const EOFException& e) {
*		cerr << "requester EOF, closing socket" << endl;
*		socket->close();
*		return RUN_OK;
*	    }
*	    cerr << "requester no EOF, closing socket" << endl;
*	    throw Exception("McThread socket read not as expected");
*	}
*       private:
*	   McSocket& client;
*       } cthread(client);
*
*       cthread.start();
*
*       Socket* socket = server.accept();
*       cerr << "server accepted, socket=" << hex << socket << endl;
*       socket->send("hello\n",7);
*       cerr << "server closing socket" << endl;
*       socket->close();
*
*       cerr << "joining cthread" << endl;
*       cthread.join();
*
*       server.close();
*       client.close();
*
    * @endcode
     */
    McSocket();

    /**
     * Copy constructor.
     */
    McSocket(const McSocket&);

    virtual ~McSocket() {}

    void setInterface(Inet4Address iaddr) {
        ifaceAddr = iaddr;
    }

    Inet4Address getInterface() const { return ifaceAddr; }

    std::list<Inet4Address> getInterfaceAddresses() const throw(IOException);

    /**
     * Get the multicast address for listening to requests.
     */
    const Inet4SocketAddress& getInet4McastSocketAddress() const
    {
        return mcastAddr;
    }

    /**
     * Set the multicast address for listening to requests.
     * @param val Multicast address to listen for McSocketDatagrams.
     */
    void setInet4McastSocketAddress(const Inet4SocketAddress& val) { mcastAddr = val; }

    /**
     * Get the pseudo port.
     */
    int getRequestNumber() const { return requestNum; }

    /**
     * Set the pseudo port.
     * @param val Pseudo port number
     */
    void setRequestNumber(int val) { requestNum = val; }

    /**
     * Register with a McSocketListener which is listening on my multicast
     * address. When a request is received on the address for my pseudoport
     * McSocketListener will call my offer() method when a socket has been
     * connected.  One either uses either form of listen() or accept()
     * to request a connection.
     */
    void listen() throw(IOException);

    /**
     * Like ServerSocket::accept(), this method will return a connected socket.
     * Register with a McSocketListener which is listening on my multicast
     * address. Then wait on a condition variable until a request is received
     * on the address for my pseudoport.  McSocketListener will call the offer()
     * method when a socket has been connected. offer() will signal the
     * condition variable, and then accept() will return with the given socket.
     */
    Socket* accept() throw(IOException);

    void request() throw(IOException);

    /**
     * Wait on an request, which will return with a real Socket when a remote
     * host has connected.
     */
    Socket* connect() throw(IOException);

    virtual void close() throw(IOException);

    /**
     * Method that is called when a socket connection is established.
     */
    virtual void connected(Socket* sock) {}

private:
    /**
     * How a McSocketListener hands me a connected TCP socket.
     * This is declared to throw Exception because a derived class
     * may start their thread at this time.  McSocket will
     * own the pointer to the socket and is responsible for closing &
     * deleting it when done.
     * @param sock A pointer to a Socket. May be null, in which case err
     *       will be a non-zero errno to be reported.
     * @param err If sock is null, an errno.
     */
    void offer(Socket* sock,int err);

    Inet4SocketAddress mcastAddr;

    Inet4Address ifaceAddr;

    int requestNum;

    Cond connectCond;

    Socket* newsocket;

    bool socketOffered;

    int offerErrno;

    /**
     * The thread we start which multicasts for connections.
     */
    Thread* multicaster;

    Mutex multicaster_mutex;
};

class McSocketListener: public Thread
{
public:

    /**
     * How a McSocket registers with a McSocketListener. If
     * a McSocketListener is not running on the multicast address
     * of the McSocket, then one is created and stared.
     * When a McSocketDatagram arrives for the McSocket, then
     * a TCP socket connection is made back to the requesting host,
     * and the connected socket passed back to the McSocket via
     * McSocket::offer();
     */
    static void accept(McSocket* sock) throw(Exception);

    static void close(McSocket* sock) throw(Exception);

    static int check() throw();

    int run() throw(Exception);

private:
    McSocketListener(const Inet4SocketAddress& addr);

    void add(McSocket* mcsocket);

    int remove(McSocket* mcsocket);

private:

    Inet4SocketAddress mcastAddr;

    Mutex mcsocket_mutex;

    std::map<int,McSocket*> mcsockets;

};

class McSocketMulticaster: public Thread
{
public:
    McSocketMulticaster(McSocket* mcsocket);
    virtual ~McSocketMulticaster();
    int run() throw(Exception);
private:
    McSocket* mcsocket;
};

}}	// namespace nidas namespace util

#endif
