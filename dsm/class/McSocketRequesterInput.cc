/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <McSocketRequesterInput.h>
#include <Datagrams.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

void McSocketRequesterInput::requestConnection(atdUtil::SocketAccepter *service,
	int pseudoPort)
    throw(atdUtil::IOException)
{
    setPseudoPort(pseudoPort);
    request(service);
}

Input* McSocketRequesterInput::clone() const
{
    return new McSocketRequesterInput(*this);
}

void McSocketRequesterInput::offer(atdUtil::Socket* sock)
	throw(atdUtil::IOException)
{
    socket = sock;
    name = socket->getInet4SocketAddress().toString();
}

size_t McSocketRequesterInput::getBufferSize() const
{
    if (socket) return socket->getReceiveBufferSize();
    else return 16384;
}

void McSocketRequesterInput::close() throw (atdUtil::IOException)
{
    atdUtil::McSocketRequester::close();
    if (socket && socket->getFd() >= 0) socket->close();
}

int McSocketRequesterInput::getFd() const
{
    if (socket) return socket->getFd();
    else return -1;
}

void McSocketRequesterInput::fromDOMElement(const xercesc::DOMElement* node)
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
	    else if (!aname.compare("type"));
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

DOMElement* McSocketRequesterInput::toDOMParent(
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

DOMElement* McSocketRequesterInput::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


