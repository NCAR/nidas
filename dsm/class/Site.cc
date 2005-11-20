/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Site.h>

#include <iostream>
#include <set>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(Site)

Site::Site()
{
}

Site::~Site()
{

    map<string,Parameter*>::iterator pi;

    for (pi = parameterMap.begin(); pi != parameterMap.end(); ++pi)
    	delete pi->second;

    // cerr << "deleting DSMServers" << endl;
    for (list<DSMServer*>::iterator is = servers.begin();
    	is != servers.end(); ++is) delete *is;

    // cerr << "deleting DSMConfigs" << endl;
    for (list<DSMConfig*>::iterator it = dsms.begin();
    	it != dsms.end(); ++it) delete *it;

}

const list<string> Site::getAllowedParameterNames() const
{
    return allowedParameterNames;
}

/**
 * Add a parameter to this Site. Site
 * will then own the pointer and will delete it
 * in its destructor.
 */
void Site::addParameter(Parameter* val)
{
    parameterMap.insert(make_pair<string,Parameter*>(
	    val->getName(),val));
    constParameters.push_back(val);
}

const Parameter* Site::getParameter(const string& name) const
{
    map<string,Parameter*>::const_iterator pi = parameterMap.find(name);
    if (pi == parameterMap.end()) return 0;
    return pi->second;
}

const list<const Parameter*>& Site::getParameters() const
{
    return constParameters;
}

void Site::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    if (xnode.getNodeName().compare("site") &&
    	xnode.getNodeName().compare("aircraft"))
	    throw atdUtil::InvalidParameterException(
		    "Site::fromDOMElement","xml node name",
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

    // keep a set of DSM ids to make sure they are unique
    set<int> dsm_ids;
    // likewise with dsm names
    set<string> dsm_names;

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
	    dsm->setSite(this);
	    dsm->fromDOMElement((DOMElement*)child);
	    addDSMConfig(dsm);
	    if (!dsm_ids.insert(dsm->getId()).second) {
		ostringstream ost;
		ost << dsm->getId();
		throw atdUtil::InvalidParameterException("dsm id",
			ost.str(),"is not unique");
	    }
	    if (!dsm_names.insert(dsm->getName()).second)
		throw atdUtil::InvalidParameterException("dsm name",
			dsm->getName(),"is not unique");
	}
	else if (!elname.compare("server")) {
	    DSMServer* server = new DSMServer();
	    server->setSite(this);
	    server->fromDOMElement((DOMElement*)child);
	    addServer(server);
	}
	else if (!elname.compare("parameter"))  {
	    Parameter* parameter =
	    	Parameter::createParameter((DOMElement*)child);
	    addParameter(parameter);
	}
    }
}

DOMElement* Site::toDOMParent(DOMElement* parent)
	throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("aircraft"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* Site::toDOMElement(DOMElement* node)
	throw(DOMException) {
    return node;
}

/**
 * Look for a server on this aircraft that either has no name or whose
 * name matches hostname.  If none found, remove any domain names
 * and try again.
 */
DSMServer* Site::findServer(const string& hostname) const
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
	const string& sname = srvr->getName();
	int sdot = sname.find('.');
	if (!sname.compare(0,sdot,hostname,0,dot)) {
	    server = srvr;
	    break;
	}
    }
    return server;
}

const DSMConfig* Site::findDSM(const atdUtil::Inet4Address& addr) const
{
    for (list<DSMConfig*>::const_iterator di=dsms.begin();
	di != dsms.end(); ++di) {
	const DSMConfig* dsm = *di;
	list<atdUtil::Inet4Address> addrs =
		atdUtil::Inet4Address::getAllByName(dsm->getName());
	for (list<atdUtil::Inet4Address>::const_iterator ai=addrs.begin();
	    ai != addrs.end(); ++ai) {
	    if (*ai == addr) return dsm;
	}
    }
    return 0;
}
