// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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
     * A McSocketUDP shouldn't be used to do any actual reads or writes,
     * it just sets up the connection. The returned IOChannel should
     * be used to read/write. Calling this method will fail with an assert.
     */
    size_t read(void*, size_t) throw (nidas::util::IOException)
    {
	assert(false);
        return 0;
    }

    /**
     * A McSocketUDP shouldn't be used to do any actual reads or writes,
     * it just sets up the connection. The returned IOChannel should
     * be used to read/write. Calling this method will fail with an assert.
     */
    size_t write(const void*, size_t) throw (nidas::util::IOException)
    {
	assert(false);
        return 0;
    }

    /**
     * A McSocketUDP shouldn't be used to do any actual reads or writes,
     * it just sets up the connection. The returned IOChannel should
     * be used to read/write. Calling this method will fail with an assert.
     */
    size_t write(const struct iovec*, int) throw (nidas::util::IOException)
    {
	assert(false);
        return 0;
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

        /** No copy, assignment. */
        MyMcSocket(const MyMcSocket&);
        MyMcSocket operator=(const MyMcSocket&);
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

    /** No assignment. */
    McSocketUDP& operator=(const McSocketUDP&);

};

}}	// namespace nidas namespace core

#endif
