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

#ifndef NIDAS_CORE_MULTIPLESOCKETUDP_H
#define NIDAS_CORE_MULTIPLESOCKETUDP_H

#include <nidas/core/McSocketUDP.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

class MultipleUDPSockets: public McSocketUDP
{
public:
    MultipleUDPSockets();

    MultipleUDPSockets(const MultipleUDPSockets&x);

    MultipleUDPSockets* clone() const;

    IOChannel* connect() throw(nidas::util::IOException);

    void connected(nidas::util::DatagramSocket* sock,
        const nidas::util::Inet4PacketInfoX& pktinfo) throw();

    size_t getBufferSize() const throw();

    size_t read(void*, size_t) throw(nidas::util::IOException)
    {
        assert(false);
	return 0;
    }

    size_t write(const void* buf, size_t len)
            throw(nidas::util::IOException);

    size_t write(const struct iovec* iov, int iovcnt)
            throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    /**
     * From the Inet4PacketInfoX associated with the received
     * request, determine if we need to create a new socket
     * to send data to the requester.
     * If this is a new destination address for unicast packets
     * add a new nidas::util::DatagramSocket. Or, if the
     * requester sent a multicast request, and it was received
     * on an interface which has not received requests before,
     * then add a new nidas::util::MulticastSocket on that interface.
     */
    void addClient(const ConnectionInfo& info);

    void removeClient(const nidas::util::Inet4SocketAddress& remoteSAddr);

    void removeClient(nidas::util::DatagramSocket*);

    void setDataPort(unsigned short val) 
    {
        _dataPortNumber = htons(val);
    }

    unsigned short getDataPort()  const
    {
        return ntohs(_dataPortNumber);
    }

private:

    void handleChangedSockets();

    mutable nidas::util::Mutex _socketMutex;

    std::list<std::pair<nidas::util::DatagramSocket*,nidas::util::Inet4SocketAddress> > _sockets;

    std::list<std::pair<nidas::util::DatagramSocket*,nidas::util::Inet4SocketAddress> > _pendingSockets;

    std::list<nidas::util::DatagramSocket*> _pendingRemoveSockets;

    /**
     * The local multicast interface of each remote client.
     */
    std::map<nidas::util::Inet4SocketAddress,nidas::util::Inet4Address>
        _multicastInterfaces;

    /**
     * Remote clients of each multicast interface.
     */
    std::map<nidas::util::Inet4Address,std::set<nidas::util::Inet4SocketAddress> >
        _multicastClients;

    /**
     * Socket of each multicast interface.
     */
    std::map<nidas::util::Inet4Address,nidas::util::DatagramSocket*>
        _multicastSockets;

    /**
     * Unicast Sockets for each destination.
     */
    std::map<nidas::util::Inet4SocketAddress,nidas::util::DatagramSocket*>
        _unicastSockets;

    bool _socketsChanged;

    unsigned short _dataPortNumber;
};


}}	// namespace nidas namespace core

#endif
