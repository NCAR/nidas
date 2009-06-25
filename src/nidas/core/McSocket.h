/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_MCSOCKET_H
#define NIDAS_CORE_MCSOCKET_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/DOMable.h>
#include <nidas/util/McSocket.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

/**
 * Implementation of an IOChannel, using nidas::util::McSocket<nidas::util::Socket> to
 * establish a TCP connection.
 */
class McSocket: public IOChannel
{
public:

    /**
     * Constructor.
     */
    McSocket();

    /**
     * Copy constructor. Should only be called before nidas::util::Socket
     * is connected.
     */
    McSocket(const McSocket&);

    /**
     * Copy constructor, with a new connnected nidas::util::Socket
     */
    // McSocket(const McSocket&,nidas::util::Socket*);

    ~McSocket() {
    }

    McSocket* clone() const;

    void setRequestType(enum McSocketRequest val) {
    	_mcsocket.setRequestType(val);
    }

    enum McSocketRequest getRequestType() const {
    	return (enum McSocketRequest) _mcsocket.getRequestType();
    }

    /**
     * Does this McSocket request connections, or does it
     * listen for incoming connections.
     */
    bool isRequester() const { return _amRequester; }

    void setRequester(bool val) { _amRequester = val; }

    void setName(const std::string& val) { _name = val; }

    const std::string& getName() const { return _name; }

    void requestConnection(IOChannelRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    virtual void connected(nidas::util::Socket* sock,const nidas::util::Inet4PacketInfoX& pktinfo);

    virtual bool isNewInput() const { return _newInput; }

    // nidas::util::Inet4Address getRemoteInet4Address();

    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
        _keepAliveIdleSecs = val;
    }

    int getKeepAliveIdleSecs() const throw (nidas::util::IOException)
    {
        return _keepAliveIdleSecs;
    }

    /**
     * Do setNonBlocking(val) on underlying socket.
     */
    void setNonBlocking(bool val) throw (nidas::util::IOException)
    {
	_nonBlocking = val;
    }

    /**
     * Return isNonBlocking() of underlying socket.
     */
    bool isNonBlocking() const throw (nidas::util::IOException)
    {
	return _nonBlocking;
    }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
        assert(false);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
        assert(false);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
        assert(false);
    }

    /**
     * Set the minimum write interval in microseconds so we don't
     * flood the network.
     * @param val Number of microseconds between physical writes.
     *        Default: 10000 microseconds (1/100 sec).
     */
    void setMinWriteInterval(int val) {
        _minWriteInterval = val;
    }

    int getMinWriteInterval() const {
        return _minWriteInterval;
    }

    void close() throw (nidas::util::IOException);

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    class MyMcSocket:  public nidas::util::McSocket<nidas::util::Socket>
    {
    public:
        MyMcSocket(nidas::core::McSocket* s) :_outer(s) {}
        void connected(nidas::util::Socket* sock,const nidas::util::Inet4PacketInfoX& pktinfo)
        {
            _outer->connected(sock,pktinfo);
        }
    private:
        nidas::core::McSocket* _outer;
    };

    void setInet4McastSocketAddress(const nidas::util::Inet4SocketAddress& val)
    {
        _mcsocket.setInet4McastSocketAddress(val);
    }

    const nidas::util::Inet4SocketAddress& getInet4McastSocketAddress() const
    {
        return _mcsocket.getInet4McastSocketAddress();
    }

protected:

    IOChannelRequester* _iochanRequester;

    MyMcSocket _mcsocket;

private:

    std::string _name;

    bool _amRequester;

    bool _firstRead;

    bool _newInput;

    int _keepAliveIdleSecs;

    /**
     * Minimum write interval in microseconds so we don't flood network.
     */
    int _minWriteInterval;

    /**
     * Time of last physical write.
     */
    dsm_time_t _lastWrite;

    bool _nonBlocking;

};


}}	// namespace nidas namespace core

#endif
