/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SocketAddress.h>
#include <Datagrams.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

void SocketAddress::fromDOMElement(const xercesc::DOMElement* node)
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
	    else if (!aname.compare("type")) stype = aval;
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute:") + aname);
	    	
	}
    }
    int port = 0;
    if (sport.length() > 0) port = atoi(sport.c_str());
    if (!stype.compare("mstream")) {
	if (saddr.length() == 0) saddr = DSM_MULTICAST_ADDR;
	if (sport.length() == 0) port = DSM_MULTICAST_PORT;
    }
    else {
	if (saddr.length() == 0) saddr = "0.0.0.0";
    }
    atdUtil::Inet4Address iaddr;
    try {
	iaddr = atdUtil::Inet4Address::getByName(saddr);
    }
    catch(const atdUtil::UnknownHostException& e) {
	throw atdUtil::InvalidParameterException(
	    "parsing XML","unknown IP address",saddr);
    }

    atdUtil::Inet4SocketAddress sockaddr = atdUtil::Inet4SocketAddress(iaddr,port);
    ((atdUtil::Inet4SocketAddress&)(*this)) = sockaddr;
}

DOMElement* SocketAddress::toDOMParent(
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

DOMElement* SocketAddress::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

