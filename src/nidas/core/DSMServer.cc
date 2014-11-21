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

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DSMService.h>

#include <nidas/core/XMLParser.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/core/SampleOutputRequestThread.h>

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/McSocket.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMServer::DSMServer(): _name(),_project(0),_site(0),
    _services(),_xmlFileName(),
    _statusSocketAddr(new n_u::Inet4SocketAddress())
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

    delete _statusSocketAddr;
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

SensorIterator DSMServer::getSensorIterator() const
{
    if (getSite()) return getSite()->getSensorIterator();
    assert(_project);
    return _project->getSensorIterator();
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
            else if (aname == "statusAddr") {
                // format:  sock:addr:port or  sock::port
                bool valOK = false;
                if (aval.length() > 5 && aval.substr(0,5) == "sock:") {
                    string::size_type colon = aval.find(':',5);

                    if (colon < string::npos) {
                        string straddr = aval.substr(5,colon-5);
                        n_u::Inet4Address addr;
                        // If no address part, it defaults to NIDAS_MULTICAST_ADDR
                        if (straddr.length() == 0) straddr = NIDAS_MULTICAST_ADDR;
                        try {
                            addr = n_u::Inet4Address::getByName(straddr);
                        }
                        catch(const n_u::UnknownHostException& e) {
                            throw n_u::InvalidParameterException(
                                string("server: ") + getName() + ": " + aname,straddr,e.what());
                        }
                        unsigned short port;
                        istringstream ist(aval.substr(colon+1));
                        ist >> port;
                        if (!ist.fail()) {
                            n_u::Inet4SocketAddress saddr(addr,port);
                            setStatusSocketAddr(saddr);
                            valOK = true;
                        }
                    }
                }
                if (!valOK) throw n_u::InvalidParameterException(
                        string("server: ") + getName(), aname,aval);
	    }
            else if (aname == "xml:base" || aname == "xmlns");
	    else throw n_u::InvalidParameterException(
		string("server") + ": " + getName(),
		"unrecognized attribute",aname);
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
	    	(xercesc::DOMElement*)child,getProject());
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

void DSMServer::scheduleServices(bool optionalProcessing) throw(n_u::Exception)
{
    assert(_project);
    _project->initSensors();
    list<DSMService*>::const_iterator si;
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;
	svc->schedule(optionalProcessing);
    }
}

void DSMServer::interruptServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;
	svc->interrupt();
    }
    SampleOutputRequestThread::getInstance()->clear();
}

void DSMServer::joinServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=_services.begin(); si != _services.end(); ++si) {
	DSMService* svc = *si;
        ILOG(("DSMServer joining %s",svc->getName().c_str()));
	svc->join();
        ILOG(("DSMServer joined %s",svc->getName().c_str()));
    }
}

