/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <McSocket.h>
#include <Datagrams.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

void McSocket::requestConnection(ConnectionRequester* requester,
	int pseudoPort)
    throw(atdUtil::IOException)
{
    connectionRequester = requester;
    setPseudoPort(pseudoPort);
    if (isRequester()) request();
    else listen();
}

IOChannel* McSocket::clone() const
{
    return new McSocket(*this);
}

void McSocket::connected(atdUtil::Socket* sock)
{
    socket = sock;
    name = socket->getInet4SocketAddress().toString();
    assert(requester);
    connectionRequester->connected(this);
}

size_t McSocket::getBufferSize() const
{
    try {
	if (socket) return socket->getReceiveBufferSize();
    }
    catch (const atdUtil::IOException& e) {}
    return 16384;
}

void McSocket::close() throw (atdUtil::IOException)
{
    atdUtil::McSocket::close();
    if (socket && socket->getFd() >= 0) socket->close();
}

int McSocket::getFd() const
{
    if (socket) return socket->getFd();
    else return -1;
}
void McSocket::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    string stype;
    string saddr;
    string sport;

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("address")) saddr = aval;
	    else if (!aname.compare("port")) sport = aval;
	    else if (!aname.compare("type")) {
		if (!aval.compare("mcaccept")) setRequester(false);
		else setRequester(true);
	    }
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute:") + aname);
	}
    }


    int port = 0;
    if (sport.length() > 0) port = atoi(sport.c_str());
    else port = DSM_MULTICAST_PORT;

    if (saddr.length() == 0) saddr = DSM_MULTICAST_ADDR;
    atdUtil::Inet4Address iaddr;
    try {
	iaddr = atdUtil::Inet4Address::getByName(saddr);
    }
    catch(const atdUtil::UnknownHostException& e) {
	throw atdUtil::InvalidParameterException(
	    "parsing XML","unknown IP address",saddr);
    }

    setInet4McastSocketAddress(atdUtil::Inet4SocketAddress(iaddr,port));
}

DOMElement* McSocket::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* McSocket::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


