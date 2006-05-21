/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <DSMService.h>
#include <Site.h>
#include <DSMServer.h>
#include <NidsIterators.h>
#include <DOMObjectFactory.h>
#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

DSMService::DSMService(const std::string& name): atdUtil::Thread(name),
	server(0),input(0)
{
    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
}

DSMService::DSMService(const DSMService& x):
	atdUtil::Thread(x),server(x.server),input(x.input)
{
}

DSMService::DSMService(const DSMService& x,SampleInputStream* newinput):
	atdUtil::Thread(x),server(x.server),input(newinput)
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
    atdUtil::Synchronized autolock(subServiceMutex);
    subServices.insert(svc);
}

void DSMService::interrupt() throw()
{
    atdUtil::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
            if (svc->isRunning()) svc->interrupt();
        }
        catch(const atdUtil::Exception& e) {
            atdUtil::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            svc->getName().c_str(),e.what());
        }
    }
    Thread::interrupt();
}

void DSMService::cancel() throw()
{
    atdUtil::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
            if (svc->isRunning()) svc->cancel();
        }
        catch(const atdUtil::Exception& e) {
            atdUtil::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            svc->getName().c_str(),e.what());
        }
    }
    try {
	if (isRunning()) Thread::cancel();
    }
    catch(const atdUtil::Exception& e) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
		"service %s: %s",
	getName().c_str(),e.what());
    }
}

int DSMService::join() throw()
{
    atdUtil::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
	    cerr << "joining " << svc->getName() << endl;
            svc->join();
	    cerr << svc->getName() << " joined" << endl;
        }
        catch(const atdUtil::Exception& e) {
            atdUtil::Logger::getInstance()->log(LOG_ERR,
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
    catch(const atdUtil::Exception& e) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
		"service %s: %s",
	getName().c_str(),e.what());
    }
    return ijoin;
}

int DSMService::checkSubServices() throw()
{
    atdUtil::Synchronized autolock(subServiceMutex);

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
            catch(const atdUtil::Exception& e) {
                atdUtil::Logger::getInstance()->log(LOG_ERR,
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

// const Site* DSMService::getSite() const
// {
//     return getDSMServer()->getSite();
// }

void DSMService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
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
        }
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
		throw atdUtil::InvalidParameterException(
		    "DSMService::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const atdUtil::Exception& e) {
                throw atdUtil::InvalidParameterException("service",
                    classattr,e.what());
            }
	    input = dynamic_cast<SampleInputStream*>(domable);
            if (!input) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
                    classattr,"is not a SampleInputStream");
	    }
            input->fromDOMElement((DOMElement*)child);
	}
        else if (!elname.compare("processor")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		    "DSMService::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const atdUtil::Exception& e) {
                atdUtil::InvalidParameterException ipe("service",
                    classattr,e.what());
		atdUtil::Logger::getInstance()->log(LOG_WARNING,"%s",ipe.what());
		continue;
            }
	    SampleIOProcessor* processor = dynamic_cast<SampleIOProcessor*>(domable);
            if (!processor) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
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
            processor->fromDOMElement((DOMElement*)child);
	    addProcessor(processor);
        }
        else throw atdUtil::InvalidParameterException(
                "DSMService::fromDOMElement",
                elname, "unsupported element");
    }
    if (!input)
        throw atdUtil::InvalidParameterException(
                "DSMService::fromDOMElement",
                "input", "no inputs specified");
    if (processors.size() == 0)
        throw atdUtil::InvalidParameterException(
                "DSMService::fromDOMElement",
                "processor", "no processors specified");
}

DOMElement* DSMService::toDOMParent(
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

DOMElement* DSMService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

