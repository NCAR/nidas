/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-05-13 11:20:32 -0600 (Wed, 13 May 2009) $

    $LastChangedRevision: 4597 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/McSocket.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_MCSOCKETUDP_H
#define NIDAS_CORE_MCSOCKETUDP_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/DOMable.h>
#include <nidas/util/McSocket.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

/**
 * Implementation of an IOChannel, using nidas::util::McSocket<nidas::util::DatagramSocket> to
 * establish a pair of communicating UDP sockets.
 */
/**
 * Implementation of an IOChannel, using McSocket to establish connections
 */
class McSocketUDP: public IOChannel {

public:

    /**
     * Constructor.
     */
    McSocketUDP();

    /**
     * Copy constructor. Should only be called before nidas::util::Socket
     * is connected.
     */
    McSocketUDP(const McSocketUDP&);

    ~McSocketUDP() { }

    McSocketUDP* clone() const;

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

    virtual bool isNewInput() const { return _newInput; }

    virtual void connected(nidas::util::DatagramSocket* sock,
        const nidas::util::Inet4PacketInfoX& pktinfo);

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

    void close() throw (nidas::util::IOException);

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    class MyMcSocket:  public nidas::util::McSocket<nidas::util::DatagramSocket>
    {
    public:
        MyMcSocket(nidas::core::McSocketUDP* s) :_outer(s) {}
        void connected(nidas::util::DatagramSocket* sock,
            const nidas::util::Inet4PacketInfoX& pktinfo)
        {
            _outer->connected(sock,pktinfo);
        }
    private:
        nidas::core::McSocketUDP* _outer;
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

    bool _nonBlocking;

};

}}	// namespace nidas namespace core

#endif
