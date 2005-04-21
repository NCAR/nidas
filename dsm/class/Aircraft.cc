/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Aircraft.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

// CREATOR_ENTRY_POINT(Aircraft)

Aircraft::Aircraft()
{
}

Aircraft::~Aircraft()
{
    // cerr << "deleting DSMServers" << endl;
    for (std::list<DSMServer*>::iterator is = servers.begin();
    	is != servers.end(); ++is) delete *is;

    // cerr << "deleting DSMConfigs" << endl;
    for (std::list<DSMConfig*>::iterator it = dsms.begin();
    	it != dsms.end(); ++it) delete *it;

}

void Aircraft::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    if (xnode.getNodeName().compare("aircraft"))
	    throw atdUtil::InvalidParameterException(
		    "Aircraft::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    if(node->hasAttributes()) {
	// get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    string aname = attr.getName();
	    string aval = attr.getValue();
	    if (!aname.compare("name")) setName(aval);
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
	// cerr << "element name=" << elname << endl;

	if (!elname.compare("dsm")) {
	    DSMConfig* dsm = new DSMConfig();
	    dsm->setAircraft(this);
	    dsm->fromDOMElement((DOMElement*)child);
	    addDSMConfig(dsm);
	}
	else if (!elname.compare("server")) {
	    DSMServer* server = new DSMServer();
	    server->setAircraft(this);
	    server->fromDOMElement((DOMElement*)child);
	    addServer(server);
	}
    }
}

DOMElement* Aircraft::toDOMParent(DOMElement* parent)
	throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("aircraft"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* Aircraft::toDOMElement(DOMElement* node)
	throw(DOMException) {
    return node;
}

/**
 * Look for a server on this aircraft that either has no name or whose
 * name matches hostname.  If none found, remove any domain names
 * and try again.
 */
DSMServer* Aircraft::findServer(const string& hostname) const
{
    DSMServer* server = 0;
    for (list<DSMServer*>::const_iterator si=servers.begin();
	si != servers.end(); ++si) {
	DSMServer* srvr = *si;
	if (srvr->getName().length() == 0 ||
	    !srvr->getName().compare(hostname)) {
	    server = srvr;
	    break;
	}
    }
    if (server) return server;

    // Not found, remove domain name, try again
    int dot = hostname.find('.');
    for (list<DSMServer*>::const_iterator si=servers.begin();
	si != servers.end(); ++si) {
	DSMServer* srvr = *si;
	const std::string& sname = srvr->getName();
	int sdot = sname.find('.');
	if (!sname.compare(0,sdot,hostname,0,dot)) {
	    server = srvr;
	    break;
	}
    }
    return server;
}

const DSMConfig* Aircraft::findDSM(const atdUtil::Inet4Address& addr) const
{
    for (list<DSMConfig*>::const_iterator di=dsms.begin();
	di != dsms.end(); ++di) {
	DSMConfig* dsm = *di;
	std::list<atdUtil::Inet4Address> addrs =
		atdUtil::Inet4Address::getAllByName(dsm->getName());
	for (list<atdUtil::Inet4Address>::const_iterator ai=addrs.begin();
	    ai != addrs.end(); ++ai) {
	    if (*ai == addr) return dsm;
	}
    }
    return 0;
}
