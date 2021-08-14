// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_MCSOCKET_H
#define NIDAS_CORE_MCSOCKET_H

#include "IOChannel.h"
#include "DOMable.h"
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

    /**
     * @throws nidas::util::IOException
     **/
    void requestConnection(IOChannelRequester* service);

    /**
     * @throws nidas::util::IOException
     **/
    IOChannel* connect();

    virtual void connected(nidas::util::Socket* sock,
                           const nidas::util::Inet4PacketInfoX& pktinfo);

    virtual bool isNewInput() const { return _newInput; }

    // nidas::util::Inet4Address getRemoteInet4Address();

    /**
     * @throws nidas::util::IOException
     **/
    void setKeepAliveIdleSecs(int val)
    {
        _keepAliveIdleSecs = val;
    }

    /**
     * @throws nidas::util::IOException
     **/
    int getKeepAliveIdleSecs() const
    {
        return _keepAliveIdleSecs;
    }

    /**
     * Do setNonBlocking(val) on underlying socket.
     *
     * @throws nidas::util::IOException
     **/
    void setNonBlocking(bool val)
    {
	_nonBlocking = val;
    }

    /**
     * Return isNonBlocking() of underlying socket.
     *
     * @throws nidas::util::IOException
     **/
    bool isNonBlocking() const
    {
	return _nonBlocking;
    }

    /**
     * A McSocket shouldn't be used to do any actual reads or writes,
     * it just sets up the connection. The returned IOChannel should
     * be used to read/write. Calling this method will fail with an assert.
     *
     * @throws nidas::util::IOException
     **/
    size_t read(void*, size_t)
    {
        assert(false);
        return 0;
    }

    /**
     * A McSocket shouldn't be used to do any actual reads or writes,
     * it just sets up the connection. The returned IOChannel should
     * be used to read/write. Calling this method will fail with an assert.
     *
     * @throws nidas::util::IOException
     **/
    size_t write(const void*, size_t)
    {
        assert(false);
        return 0;
    }

    /**
     * A McSocket shouldn't be used to do any actual reads or writes,
     * it just sets up the connection. The returned IOChannel should
     * be used to read/write. Calling this method will fail with an assert.
     *
     * @throws nidas::util::IOException
     **/
    size_t write(const struct iovec*, int)
    {
        assert(false);
        return 0;
    }

    /**
     * @throws nidas::util::IOException
     **/
    void close();

    int getFd() const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);

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
        MyMcSocket(const MyMcSocket&);
        MyMcSocket& operator=(const MyMcSocket&);
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

    /**
     * Copy constructor. Should only be called before nidas::util::Socket
     * is connected.
     */
    McSocket(const McSocket&);

    IOChannelRequester* _iochanRequester;

    MyMcSocket _mcsocket;

private:

    std::string _name;

    bool _amRequester;

    bool _firstRead;

    bool _newInput;

    int _keepAliveIdleSecs;

    bool _nonBlocking;

    /**
     * No assignment.
     */
    McSocket& operator=(const McSocket&);
};


}}	// namespace nidas namespace core

#endif
