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

#include <nidas/core/MultipleUDPSockets.h>
#include <nidas/core/DatagramSocket.h>
#include <nidas/util/Process.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

MultipleUDPSockets::MultipleUDPSockets():
    _socketMutex(),_sockets(),_pendingSockets(),_pendingRemoveSockets(),
    _multicastInterfaces(),_multicastClients(),
    _multicastSockets(), _unicastSockets(),
    _socketsChanged(false),
    _dataPortNumber(NIDAS_DATA_PORT_UDP)
{
    setName("MultipleUDPSockets");
}

/*
 * Copy constructor. Should only be called before socket connection.
 */
MultipleUDPSockets::MultipleUDPSockets(const MultipleUDPSockets& x):
    McSocketUDP(x),
    _socketMutex(),_sockets(),_pendingSockets(),_pendingRemoveSockets(),
    _multicastInterfaces(),_multicastClients(),
    _multicastSockets(), _unicastSockets(),
    _socketsChanged(false),_dataPortNumber(x._dataPortNumber)
{
    setName("MultipleUDPSockets");
}

MultipleUDPSockets* MultipleUDPSockets::clone() const
{
    return new MultipleUDPSockets(*this);
}

IOChannel* MultipleUDPSockets::connect()
    throw(n_u::IOException)
{
    n_u::DatagramSocket* sock;
    n_u::Inet4PacketInfoX pktinfo;

    sock = _mcsocket.accept(pktinfo);

    nidas::core::DatagramSocket* ncsock = new nidas::core::DatagramSocket(sock);

    ConnectionInfo info(pktinfo.getRemoteSocketAddress(),
        pktinfo.getDestinationAddress(),pktinfo.getInterface());
    ncsock->setConnectionInfo(info);
    ncsock->setNonBlocking(isNonBlocking());
    addClient(info);
    return ncsock;
}

void MultipleUDPSockets::connected(n_u::DatagramSocket* sock,
    const n_u::Inet4PacketInfoX& pktinfo) throw()
{
    ILOG(("MultipleUDPSockets::connected"));
    ConnectionInfo info(pktinfo.getRemoteSocketAddress(),
        pktinfo.getDestinationAddress(),pktinfo.getInterface());

    nidas::core::DatagramSocket* dsock = new nidas::core::DatagramSocket(sock);
    dsock->setConnectionInfo(info);
    dsock->setNonBlocking(isNonBlocking());

    assert(_iochanRequester);
    _iochanRequester->connected(dsock); // notify UDPSampleOutput

    // addClient(info);
}

void MultipleUDPSockets::addClient(const ConnectionInfo& info)
{
    
    // If client sent the original request to our multicast address then
    // we assume they want multicast data, otherwise unicast.
    n_u::Inet4Address destAddr = info.getDestinationAddress();
    n_u::Inet4SocketAddress remoteSAddr = info.getRemoteSocketAddress();
#ifdef DEBUG
     cerr << "destAddr=" << destAddr.getHostAddress() << " multicast=" <<
         destAddr.isMultiCastAddress() << endl;
#endif

    if (destAddr.isMultiCastAddress()) {
        n_u::Inet4Address ifaceAddr = info.getInterface().getAddress();
        n_u::Inet4SocketAddress mcsaddr = getInet4McastSocketAddress();

        mcsaddr.setPort(getDataPort());

        ILOG(("adding multicast client ") << remoteSAddr.toAddressString() <<
            " to " << mcsaddr.toAddressString());

        _socketMutex.lock();

        // check if we have a socket already serving this interface
        n_u::DatagramSocket* dsock = _multicastSockets[ifaceAddr];
        if (!dsock) {

            ILOG(("adding multicast socket to ") << mcsaddr.toAddressString() << 
                " on iface " << ifaceAddr.getHostAddress());

            n_u::MulticastSocket* msock = new n_u::MulticastSocket();
            msock->setMulticastLoop(true);
            dsock = msock;
            msock->setInterface(mcsaddr.getInet4Address(),info.getInterface());

            // connecting to the multicast address and port seemed to generate
            // "connection refused" messages. Needs a bit more investigaton.
            // Instead of connecting we'll do sendto()'s.
            // msock->connect(mcsaddr);

            _pendingSockets.push_back(
                pair<n_u::DatagramSocket*,n_u::Inet4SocketAddress>(msock,mcsaddr));

            _multicastSockets[ifaceAddr] = msock;
            _socketsChanged = true;
        }
        _multicastInterfaces[remoteSAddr] = ifaceAddr;
        set<n_u::Inet4SocketAddress> clients = _multicastClients[ifaceAddr];
        clients.insert(remoteSAddr);
        _multicastClients[ifaceAddr] = clients;
        _socketMutex.unlock();
    }
    else {
        _socketMutex.lock();

        n_u::DatagramSocket* dsock = _unicastSockets[remoteSAddr];
        if (!dsock) {
            dsock = new n_u::DatagramSocket();

            // set default destination address and port
            // dsock->connect(remoteSAddr);
            //
            _pendingSockets.push_back(
                pair<n_u::DatagramSocket*,n_u::Inet4SocketAddress>(dsock,remoteSAddr));
            _unicastSockets[remoteSAddr] = dsock;
            _socketsChanged = true;
        }
        _socketMutex.unlock();
    }
}

void MultipleUDPSockets::removeClient(const n_u::Inet4SocketAddress& remoteSAddr)
{
    bool foundClient = false;
    DLOG(("removeClient by socket address=") << remoteSAddr.toAddressString());

    n_u::Autolock al(_socketMutex);
    n_u::DatagramSocket* dsock = _unicastSockets[remoteSAddr];
    if (dsock) {
        _pendingRemoveSockets.push_back(dsock);
        _unicastSockets[remoteSAddr] = 0;
        _socketsChanged = true;
        foundClient = true;
    }
    else {
        map<n_u::Inet4SocketAddress,n_u::Inet4Address>::const_iterator mi =
            _multicastInterfaces.find(remoteSAddr);

        if (mi != _multicastInterfaces.end()) {
            // pair<n_u::Inet4SocketAddress,n_u::Inet4Address> p = *mi;
            n_u::Inet4Address ifaceAddr = mi->second;
            set<n_u::Inet4SocketAddress> clients = _multicastClients[ifaceAddr];
#ifdef CHECK_IF_EXISTS
            set<n_u::Inet4SocketAddress>::const_iterator ci =  clients.find(remoteSAddr);
            if (ci != clients.end()) clients.erase(ci);
            foundClient = true;
#else
            size_t nc = clients.size();
            clients.erase(remoteSAddr);
            if (clients.size() < nc) foundClient = true;
#endif
            _multicastClients[ifaceAddr] = clients;
            if (clients.size() == 0) {
                n_u::DatagramSocket* dsock = _multicastSockets[ifaceAddr];
                if (dsock) {
                    _pendingRemoveSockets.push_back(dsock);
                    // _multicastSockets[ifaceAddr] = 0;
                    _multicastSockets.erase(ifaceAddr);
                    _socketsChanged = true;
                }
            }
        }
    }
    if (!foundClient) ILOG(("MultipleUDPSockets, client ") <<
        remoteSAddr.toAddressString() << " not found");
}

void MultipleUDPSockets::removeClient(n_u::DatagramSocket* dsock)
{
    cerr << "removeClient by socket" << endl;

    n_u::Autolock al(_socketMutex);

    map<n_u::Inet4SocketAddress,n_u::DatagramSocket*>::iterator di;

    for (di = _unicastSockets.begin(); di != _unicastSockets.end(); ++di) {
        if (di->second == dsock) {
            _pendingRemoveSockets.push_back(dsock);
            _unicastSockets.erase(di);
            _socketsChanged = true;
            return;
        }
    }

    map<n_u::Inet4Address,n_u::DatagramSocket*>::iterator mi =
        _multicastSockets.begin();
    for ( ; mi != _multicastSockets.end(); ++mi) {
        if (mi->second == dsock) {
            n_u::Inet4Address iface = mi->first;
            // remote socket addresses serviced by multicasts on iface
            set<n_u::Inet4SocketAddress> sas = _multicastClients[iface];
            set<n_u::Inet4SocketAddress>::const_iterator si = sas.begin();
            for ( ; si != sas.end(); ++si) {
                n_u::Inet4SocketAddress sa = *si;
                /*
                map<n_u::Inet4SocketAddress,n_u::Inet4Address>::iterator mmi;
                mmi = _multicastInterfaces.find(sa);
                if (mmi != _multicastInterfaces.end()) _multicastInterfaces.erase(mmi);
                */
                _multicastInterfaces.erase(sa);
            }
            _pendingRemoveSockets.push_back(dsock);
            _multicastSockets.erase(mi);
            _socketsChanged = true;
            return;
        }
    }
    ILOG(("MultipleUDPSockets, removeClient socket not found "));
}

size_t MultipleUDPSockets::getBufferSize() const throw()
{
    size_t blen = 16384;
    try {
        n_u::Autolock al(_socketMutex);
        if (!_sockets.empty()) {
            n_u::DatagramSocket* sock = _sockets.front().first;
            blen = sock->getReceiveBufferSize();
        }
    }
    catch (const n_u::IOException& e) {}
    if (blen > 16384) blen = 16384;
    return blen;
}

void MultipleUDPSockets::handleChangedSockets()
{
    _socketMutex.lock();
    // remove sockets
    for (list<n_u::DatagramSocket*>::const_iterator si =
        _pendingRemoveSockets.begin(); si != _pendingRemoveSockets.end(); ++si) {
        n_u::DatagramSocket* dsock = *si;

        list<pair<n_u::DatagramSocket*,n_u::Inet4SocketAddress> >::iterator si2 =
            _sockets.begin();
        for ( ; si2 != _sockets.end(); ++si2 ) {
            if (si2->first == dsock) {
                _sockets.erase(si2);
                break;
            }
        }
        if (si2 == _sockets.end()) WLOG(("Cannot remove socket for ") << dsock->getLocalSocketAddress().toAddressString());
    }
    _pendingRemoveSockets.clear();
    // docs say splice removes elements from the second list
    _sockets.splice(_sockets.end(),_pendingSockets);
    _socketsChanged = false;
    _socketMutex.unlock();
}

size_t MultipleUDPSockets::write(const void* buf, size_t len)
            throw(n_u::IOException)
{
    size_t res = 0;
    if (_socketsChanged) handleChangedSockets();

    list<pair<n_u::DatagramSocket*,n_u::Inet4SocketAddress> >::const_iterator si =  _sockets.begin();
    for ( ; si != _sockets.end(); ++si) {
        n_u::DatagramSocket* dsock = si->first;
        try {
            res = dsock->sendto(buf,len,0,si->second);
            // since we did a connect, we don't have to do a sendto() a specific address
            // res = dsock->send(buf,len,0);
        }
        catch(const n_u::IOException& e) {
            ILOG(("%s",e.what()));
            removeClient(dsock);
        }
    }
    return res;
}

size_t MultipleUDPSockets::write(const struct iovec* iov, int iovcnt)
            throw(n_u::IOException)
{
    size_t res = 0;
    if (_socketsChanged) handleChangedSockets();

    list<pair<n_u::DatagramSocket*,n_u::Inet4SocketAddress> >::const_iterator si =  _sockets.begin();
    for ( ; si != _sockets.end(); ++si) {
        n_u::DatagramSocket* dsock = si->first;
        try {
            res = dsock->sendto(iov,iovcnt,0,si->second);
            // res = dsock->send(iov,iovcnt,0);
        }
        catch(const n_u::IOException& e) {
            ILOG(("%s",e.what()));
            removeClient(dsock);
        }
    }

    return res;
}

void MultipleUDPSockets::close()
            throw(n_u::IOException)
{
    if (_socketsChanged) {
        _socketMutex.lock();
        _sockets.splice(_sockets.end(),_pendingSockets);
        _socketsChanged = false;
        _socketMutex.unlock();
    }
    list<pair<n_u::DatagramSocket*,n_u::Inet4SocketAddress> >::const_iterator si =  _sockets.begin();
    for ( ; si != _sockets.end(); ++si) {
        n_u::DatagramSocket* dsock = si->first;
        dsock->close();
    }
}

int MultipleUDPSockets::getFd() const
{
    n_u::Autolock al(_socketMutex);
    if (!_sockets.empty()) {
        n_u::DatagramSocket* sock = _sockets.front().first;
        return sock->getFd();
    }
    else return -1;
}

void MultipleUDPSockets::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    string sport;
    int port = NIDAS_SVC_REQUEST_PORT_UDP;
    string saddr = NIDAS_MULTICAST_ADDR;
    setRequester(false);
    setRequestType(UDP_PROCESSED_SAMPLE_FEED);

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const string& aname = attr.getName();
            const string& aval = attr.getValue();
	    if (aname == "address") saddr = n_u::Process::expandEnvVars(aval);
	    else if (aname == "port") sport = n_u::Process::expandEnvVars(aval);
	    else if (aname == "requestType") {
		int i;
	        istringstream ist(aval);
		ist >> i;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			    getName(),aname,aval);
		setRequestType((enum McSocketRequest)i);
	    }
	    else if (aname == "type") {
		if (aval == "dataUDP");
		else throw n_u::InvalidParameterException(
			getName(),"type",aval);
	    }
	    else if (aname == "block") {
		istringstream ist(aval);
		ist >> boolalpha;
		bool val;
		ist >> val;
		if (ist.fail())
			throw n_u::InvalidParameterException(
			    "socket","block",aval);
		setNonBlocking(!val);
	    }
	    else throw n_u::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }

    // Default address for multicast requesters and accepters
    // is NIDAS_MULTICAST_ADDR.
    if (saddr.length() == 0) 
	throw n_u::InvalidParameterException(
	    	getName(),"address","unknown address for dataUDP socket");

    n_u::Inet4Address iaddr;
    try {
	iaddr = n_u::Inet4Address::getByName(saddr);
    }
    catch(const n_u::UnknownHostException& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,"socket: unknown IP address: %s",
            saddr.c_str());
    }
    if (sport.length() > 0) port = atoi(sport.c_str());

    setInet4McastSocketAddress(n_u::Inet4SocketAddress(iaddr,port));
    // cerr << "socket address=" << getInet4McastSocketAddress().toString() << endl;
}
