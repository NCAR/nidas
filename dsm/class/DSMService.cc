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
#include <Aircraft.h>
#include <DSMServer.h>
#include <DOMObjectFactory.h>
#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

DSMService::DSMService(const std::string& name): atdUtil::Thread(name),
	server(0)
{
    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
}

DSMService::~DSMService()
{
    // cerr << "~DSMService" << endl;
}

void DSMService::addSubService(DSMService* svc) throw()
{
    atdUtil::Synchronized autolock(subServiceMutex);
    subServices.insert(svc);
}

void DSMService::interruptSubServices() throw()
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
}

void DSMService::cancelSubServices() throw()
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
}

void DSMService::joinSubServices() throw()
{
    atdUtil::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        DSMService* svc = *si;
        try {
            svc->join();
        }
        catch(const atdUtil::Exception& e) {
            atdUtil::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            svc->getName().c_str(),e.what());
        }
	delete svc;
    }
    subServices.clear();
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
                svc->getName() << " not running" << endl;
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

const Aircraft* DSMService::getAircraft() const
{
    return getDSMServer()->getAircraft();
}

void DSMService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
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
	}
    }
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

