//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_DATAGRAMPACKET_H
#define NIDAS_UTIL_DATAGRAMPACKET_H

#include <nidas/util/Inet4SocketAddress.h>

#include <sys/param.h>	// MIN

namespace nidas { namespace util {

/**
 * Abstract base class for a UDP datagram.  Patterned after java.net.DatagramPacket.
 */
class DatagramPacketBase {
public:

    /**
     * Create a DatagramPacketBase for receiving.
     * @param buf allocated space for contents. Pointer is owned
     *            by caller, not by the DatagramPacketBase - the
     *            DatagramPacketBase destructor does not delete it
     *            in its destructor.
     * @param length length of allocated space.
     */
    DatagramPacketBase(int l) :
    	maxlength(l),length(l),addr(new Inet4SocketAddress()) {}

    /**
     * Create a DatagramPacketBase for sending.
     * @param buf allocated space for contents. Pointer is owned
     *            by caller, not by the DatagramPacketBase - the
     *            DatagramPacketBase destructor does not delete it
     *            in its destructor.
     * @param length length of allocated space.
     * @add Inet4Address to send packet to.
     * @port port number to send packet to.
     */
    DatagramPacketBase(int l, const Inet4Address& add, int port):
    	maxlength(l),length(l),
		addr(new Inet4SocketAddress(add,port)) {}

    DatagramPacketBase(int l, const SocketAddress& add):
    	maxlength(l),length(l),addr(add.clone()) {}

    /**
     * Copy constructor.
     */ 
    DatagramPacketBase(const DatagramPacketBase& x) :
    	maxlength(x.maxlength),length(x.length),addr(x.addr->clone())
    {
    }

    /**
     * Assignment operator.
     */ 
    DatagramPacketBase& operator=(const DatagramPacketBase& rhs)
    {
    	maxlength = rhs.maxlength;
	length = rhs.length;
	delete addr;
	addr = rhs.addr->clone();
	return *this;
    }

    /**
     * Virtual destructor.
     */
    virtual ~DatagramPacketBase()
    {
        delete addr;
    }

    SocketAddress& getSocketAddress() const { return *addr; }

    void setSocketAddress(const SocketAddress& val)
    {
        SocketAddress* oldaddr = addr;
        addr = val.clone();
	delete oldaddr;
    }

    struct sockaddr* getSockAddrPtr() { return addr->getSockAddrPtr(); }

    const struct sockaddr* getConstSockAddrPtr() const {
    	return addr->getConstSockAddrPtr();
    }

    int getSockAddrLen() const { return addr->getSockAddrLen(); }

    /**
     * Get the pointer to the data portion of the packet.
     */
    virtual void* getDataVoidPtr() = 0;

    virtual const void* getConstDataVoidPtr() const = 0;

    /**
     * Set the value for the current number of bytes in data.
     * /param val Number of bytes, must be <= max length.
     */
    virtual void setLength(int val) { length = val; }

    /**
     * Get the value for the current number of bytes in data.
     * Either the number of bytes read in a received packet,
     * or the number of bytes to send().
     */
    virtual int getLength() const { return length; }

    /**
     * Set the allocated length in bytes of the data.
     * /param val Number of bytes allocated in data.
     */
    void setMaxLength(int val) { maxlength = val; }

    /**
     * Return the allocated length in bytes of the data.
     */
    int getMaxLength() const { return maxlength; }

protected:
    /**
     * allocated length in bytes of contents.
     */
    int maxlength;

    /**
     * length of datagram that was read or will be sent. Must be
     * <= maxlength.
     */
    int length;

    /**
     * The destination address for a send, or the remote senders
     * address after a receive.
     */
    SocketAddress* addr;

};

/**
 * A DatagramPacket with a specific structure of data.
 * DatagramPacketT doesn not own the pointer to the data.
 * 
 * This class provides (default) copy constructors and assignment
 * operators. The data pointer of the new copies will point to the
 * same data as the original. The user is responsible for making
 * sure that pointer is valid and the space deallocated when finished.
 */
template <class DataT>
class DatagramPacketT: public DatagramPacketBase {
public:
    DatagramPacketT(DataT* buf, int n) :
    	DatagramPacketBase(n * sizeof(DataT)),data(buf) {}

    /**
     * Create a DatagramPacket for sending.
     * @param buf allocated space for contents. Pointer is owned
     *            by caller, not by the DatagramPacket - the
     *            DatagramPacket destructor does not delete it
     *            in its destructor.
     * @param length length of allocated space.
     * @add Inet4Address to send packet to.
     * @port port number to send packet to.
     */
    DatagramPacketT(DataT* buf, int n, const Inet4Address& add, int port):
    	DatagramPacketBase(n * sizeof(DataT),add,port),data(buf) {}

    DatagramPacketT(DataT* buf, int n, const SocketAddress& add):
    	DatagramPacketBase(n * sizeof(DataT),add),data(buf) {}


    /**
     * Get the pointer to the data portion of the packet.
     */
    virtual void* getDataVoidPtr() { return data; }

    virtual const void* getConstDataVoidPtr() const { return data; }

    DataT* getData() { return data; }

    /**
     * Set the pointer to the data portion of the packet.
     * /param val Pointer to an allocated section of memory.
     * This pointer is owned by the caller, it is not deleted
     * by the DatagramPacket destructor.
     */

    void setData(DataT *val) { data = val; }

protected:
    DataT* data;

};

class DatagramPacket: public DatagramPacketT<char>
{
public:
    DatagramPacket(char* buf, int length) :
    	DatagramPacketT<char>(buf,length) {}

    /**
     * Create a DatagramPacket for sending.
     * @param buf allocated space for contents. Pointer is owned
     *            by caller, not by the DatagramPacket - the
     *            DatagramPacket destructor does not delete it
     *            in its destructor.
     * @param length length of allocated space.
     * @add Inet4Address to send packet to.
     * @port port number to send packet to.
     */
    DatagramPacket(char* buf, int length, const Inet4Address& add, int port):
    	DatagramPacketT<char>(buf,length,add,port) {}

    DatagramPacket(char* buf, int length, const SocketAddress& add):
    	DatagramPacketT<char>(buf,length,add) {}
};

} }	// namespace nidas namespace util

#endif
