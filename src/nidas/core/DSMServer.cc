/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMServer.h>

#include <nidas/core/Site.h>

#include <nidas/core/DSMTime.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DOMObjectFactory.h>

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/McSocket.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMServer::DSMServer()
{
}

DSMServer::~DSMServer()
{
    // delete services.
    list<DSMService*>::const_iterator si;
#ifdef DEBUG
    cerr << "~DSMServer services.size=" << _services.size() << endl;
#endif
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;
#ifdef DEBUG
	cerr << "~DSMServer: deleting " << svc->getName() << endl;
#endif
	delete svc;
    }
#ifdef DEBUG
    cerr << "~DSMServer: deleted services " << endl;
#endif
}
#undef DEBUG

DSMServiceIterator DSMServer::getDSMServiceIterator() const
{
    return DSMServiceIterator(this);
}

ProcessorIterator DSMServer::getProcessorIterator() const
{
    return ProcessorIterator(this);
}

SiteIterator DSMServer::getSiteIterator() const
{
    return SiteIterator(this);
}

DSMConfigIterator DSMServer::getDSMConfigIterator() const
{
    return DSMConfigIterator(this);
}

SensorIterator DSMServer::getSensorIterator() const
{
    return SensorIterator(this);
}

SampleTagIterator DSMServer::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

void DSMServer::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
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
	    if (aname == "name") setName(aval);
	}
    }
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();
	if (elname == "service") {
	    const string classattr = DSMService::getClassName(
	    	(xercesc::DOMElement*)child);
	    if (classattr.length() == 0) 
		throw n_u::InvalidParameterException(
		    "DSMServer::fromDOMElement",
			elname,
			"does not have a class attribute");
	    DOMable* domable;
	    try {
		domable = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const n_u::Exception& e) {
		throw n_u::InvalidParameterException("service",
		    classattr,e.what());
	    }
	    DSMService* service = dynamic_cast<DSMService*>(domable);
	    if (!service) {
		delete domable;
		throw n_u::InvalidParameterException("service",
		    classattr,"is not of type DSMService");
	    }
	    service->setDSMServer(this);
	    service->fromDOMElement((xercesc::DOMElement*)child);
	    addService(service);
	}
    }
}

void DSMServer::scheduleServices() throw(n_u::Exception)
{
    list<DSMService*>::const_iterator si;
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;

	SiteIterator si = getSiteIterator();
	for ( ; si.hasNext(); ) {
	    const Site* site = si.next();
	    DSMConfigIterator di =
		site->getDSMConfigIterator();
	    for ( ; di.hasNext(); ) {
		const DSMConfig* dsm = di.next();
		Project::getInstance()->initSensors(dsm);
	    }
	}
	svc->schedule();
    }
}

void DSMServer::interruptServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;
	svc->interrupt();
    }
}

void DSMServer::joinServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;
	svc->join();
    }
}

