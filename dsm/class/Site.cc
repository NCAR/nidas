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
#include <DSMServer.h>

#include <iostream>
#include <set>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(Site)

Site::Site(): number(-1)
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
    for (list<DSMConfig*>::iterator it = ncDsms.begin();
    	it != ncDsms.end(); ++it) delete *it;

}

DSMConfigIterator Site::getDSMConfigIterator() const
{
    return DSMConfigIterator(this);
}

SensorIterator Site::getSensorIterator() const
{
    return SensorIterator(this);
}

SampleTagIterator Site::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator Site::getVariableIterator() const
{
    return VariableIterator(this);
}

/**
 * Initialize all sensors for a Site.
 */
void Site::initSensors() throw(atdUtil::IOException)
{
    list<const DSMConfig*>::const_iterator di = getDSMConfigs().begin();
    for ( ; di != getDSMConfigs().end(); ++di) {
	DSMConfig* ncdsm = const_cast<DSMConfig*>(*di);
    	ncdsm->initSensors();
    }
}

/**
 * Initialize all sensors for a given dsm.
 */
void Site::initSensors(const DSMConfig* dsm) throw(atdUtil::IOException)
{
    list<const DSMConfig*>::const_iterator di = getDSMConfigs().begin();
    for ( ; di != getDSMConfigs().end(); ++di) {
	DSMConfig* ncdsm = const_cast<DSMConfig*>(*di);
    	if (ncdsm == dsm) ncdsm->initSensors();
    }
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
    if (xnode.getNodeName() != "site" &&
    	xnode.getNodeName() != "aircraft")
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
	    if (aname == "name") setName(aval);
	    else if (aname == "suffix") setSuffix(aval);
	    else if (aname == "number") {
	        istringstream ist(aval);
		int num;
		ist >> num;
		if (ist.fail()) 
		    throw atdUtil::InvalidParameterException(
		    	((getName().length() == 0) ? "site" : getName()),
				aname,aval);
		setNumber(num);
	    }
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

	if (elname == "dsm") {
	    DSMConfig* dsm = new DSMConfig();
	    dsm->setSite(this);
	    dsm->setSiteSuffix(getSuffix());
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
	else if (elname == "server") {
	    DSMServer* server = new DSMServer();
	    server->fromDOMElement((DOMElement*)child);
	    server->addSite(this);
	    addServer(server);
	}
	else if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((DOMElement*)child);
	    addParameter(parameter);
	}
    }

    // likewise with variables.
    set<Variable> varset;
    for (VariableIterator vi = getVariableIterator(); vi.hasNext(); ) {
	const Variable* var = vi.next();
	if (!varset.insert(*var).second)
	    throw atdUtil::InvalidParameterException("variable",
		var->getName(),"is not unique");
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
	    srvr->getName() == hostname) {
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
    for (list<const DSMConfig*>::const_iterator di=dsms.begin();
	di != dsms.end(); ++di) {
	const DSMConfig* dsm = *di;
#ifdef DEBUG
	cerr << "Checking dsm " << dsm->getName() << endl;
#endif
	list<atdUtil::Inet4Address> addrs =
		atdUtil::Inet4Address::getAllByName(dsm->getName());
	for (list<atdUtil::Inet4Address>::const_iterator ai=addrs.begin();
	    ai != addrs.end(); ++ai) {
	    if (*ai == addr) return dsm;
	}
    }
    return 0;
}

const DSMConfig* Site::findDSM(unsigned long id) const
{
    for (list<const DSMConfig*>::const_iterator di=dsms.begin();
	di != dsms.end(); ++di) {
	const DSMConfig* dsm = *di;
#ifdef DEBUG
	cerr << "Checking dsm " << dsm->getName() << " for id=" << id << endl;
#endif
	if (dsm->getId() == id) return dsm;
    }
    return 0;
}

DSMSensor* Site::findSensor(unsigned long id) const
{
    SensorIterator si = getSensorIterator();
    for ( ; si.hasNext(); ) {
	DSMSensor* sensor = si.next();
#ifdef DEBUG
	cerr << "Site::findSensor, getId=" << hex << sensor->getId() <<
		" against id=" << id << dec << endl;
#endif
	if (sensor->getId() == id) return sensor;
    }
    return 0;
}
