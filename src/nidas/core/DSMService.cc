/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/DSMService.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

using nidas::dynld::SampleInputStream;

namespace n_u = nidas::util;

DSMService::DSMService(const std::string& name): n_u::Thread(name),
	server(0),input(0)
{
    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
}

DSMService::DSMService(const DSMService& x):
	n_u::Thread(x),server(x.server),input(x.input)
{
}

DSMService::DSMService(const DSMService& x,SampleInputStream* newinput):
	n_u::Thread(x),server(x.server),input(newinput)
{
}

DSMService::~DSMService()
{
    if (input) {
	// cerr << "~DSMService, closing " << input->getName() << endl;
        input->close();
	delete input;
    }
#ifdef DEBUG
    cerr << "~DSMService, deleting processors" << endl;
#endif
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = processors.begin(); pi != processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
#ifdef DEBUG
	cerr << "~DSMService, deleting " <<
	    processor->getName() << endl;
#endif
	delete processor;
    }
    // cerr << "~DSMService" << endl;
}

ProcessorIterator DSMService::getProcessorIterator() const
{
    return ProcessorIterator(this);
}

void DSMService::setDSMServer(DSMServer* val)
{
    server = val;
}

void DSMService::addSubService(DSMService* svc) throw()
{
    n_u::Synchronized autolock(subServiceMutex);
    subServices.insert(svc);
}

void DSMService::interrupt() throw()
{
    n_u::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
            if (svc->isRunning()) svc->interrupt();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            svc->getName().c_str(),e.what());
        }
    }
    Thread::interrupt();
}

void DSMService::cancel() throw()
{
    n_u::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
            if (svc->isRunning()) svc->cancel();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            svc->getName().c_str(),e.what());
        }
    }
    try {
	if (isRunning()) Thread::cancel();
    }
    catch(const n_u::Exception& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
		"service %s: %s",
	getName().c_str(),e.what());
    }
}

int DSMService::join() throw()
{
    n_u::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
	    cerr << "joining " << svc->getName() << endl;
            svc->join();
	    cerr << svc->getName() << " joined" << endl;
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            svc->getName().c_str(),e.what());
        }
	delete svc;
    }
    subServices.clear();
    int ijoin = 0;
    try {
	if (!isJoined()) ijoin = Thread::join();
    }
    catch(const n_u::Exception& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
		"service %s: %s",
	getName().c_str(),e.what());
    }
    return ijoin;
}

int DSMService::checkSubServices() throw()
{
    n_u::Synchronized autolock(subServiceMutex);

    int nrunning = 0;
    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ) {
        DSMService* svc = *si;
        if (!svc->isRunning()) {
            cerr << "DSMService::checkSubServices " <<
                " joining " << svc->getName() << endl;
            try {
                svc->join();
            }
            catch(const n_u::Exception& e) {
                n_u::Logger::getInstance()->log(LOG_ERR,
                        "thread %s has quit, exception=%s",
                svc->getName().c_str(),e.what());
            }
            delete svc;
            subServices.erase(si);
            si = subServices.begin();
        }
        else {
            nrunning++;
            ++si;
        }
    }
    return nrunning;
}

/* static */
const string DSMService::getClassName(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& idref = xnode.getAttributeValue("IDREF");
    if (idref.length() > 0) {
	Project* project = Project::getInstance();
	if (!project->getServiceCatalog())
	    throw n_u::InvalidParameterException(
		"service",
		"cannot find servicecatalog for service with IDREF",
		idref);

	map<string,xercesc::DOMElement*>::const_iterator mi;
	mi = project->getServiceCatalog()->find(idref);
	if (mi == project->getServiceCatalog()->end())
		throw n_u::InvalidParameterException(
	    "service",
	    "servicecatalog does not contain a service with ID",
	    idref);
	const string classattr = getClassName(mi->second);
	if (classattr.length() > 0) return classattr;
    }
    return xnode.getAttributeValue("class");
}

void DSMService::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& idref = xnode.getAttributeValue("IDREF");
    if (idref.length() > 0) {
        Project* project = Project::getInstance();
        if (!project->getServiceCatalog())
            throw n_u::InvalidParameterException(
                project->getName(),
                "cannot find servicecatalog for service with IDREF",
                idref);

        map<string,xercesc::DOMElement*>::const_iterator mi;

        mi = project->getServiceCatalog()->find(idref);
        if (mi == project->getServiceCatalog()->end())
                throw n_u::InvalidParameterException(
	    project->getName(),
            "servicecatalog does not contain a service with ID",
            idref);
        // read catalog entry
        fromDOMElement(mi->second);
    }

    // process <input> and <processor> child elements
    xercesc::DOMNode* child = 0;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	DOMable* domable;
        if (!elname.compare("input")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw n_u::InvalidParameterException(
		    "DSMService::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const n_u::Exception& e) {
                throw n_u::InvalidParameterException("service",
                    classattr,e.what());
            }
	    input = dynamic_cast<SampleInputStream*>(domable);
            if (!input) {
		delete domable;
                throw n_u::InvalidParameterException("service",
                    classattr,"is not a SampleInputStream");
	    }
            input->fromDOMElement((xercesc::DOMElement*)child);
	}
        else if (!elname.compare("processor")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw n_u::InvalidParameterException(
		    "DSMService::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const n_u::Exception& e) {
                n_u::InvalidParameterException ipe("service",
                    classattr,e.what());
		n_u::Logger::getInstance()->log(LOG_WARNING,"%s",ipe.what());
		continue;
            }
	    SampleIOProcessor* processor = dynamic_cast<SampleIOProcessor*>(domable);
            if (!processor) {
		delete domable;
                throw n_u::InvalidParameterException("service",
                    classattr,"is not of type SampleIOProcessor");
	    }
	    // set the DSMId if we're associated with only one DSM.
	    if (getDSMServer() && getDSMServer()->getSites().size() == 1) {
	        Site* site = getDSMServer()->getSites().front();
		if (site->getDSMConfigs().size() == 1)
		    processor->setDSMId(
		    	site->getDSMConfigs().front()->getId());
	    }
	    processor->setService(this);
            processor->fromDOMElement((xercesc::DOMElement*)child);
	    addProcessor(processor);
        }
        else throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                elname, "unsupported element");
    }
    if (!input)
        throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                "input", "no inputs specified");
    if (processors.size() == 0)
        throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                "processor", "no processors specified");
}

xercesc::DOMElement* DSMService::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* DSMService::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

