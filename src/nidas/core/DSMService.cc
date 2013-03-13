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
#include <nidas/core/DSMService.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/SampleInput.h>
#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/core/ServiceCatalog.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMService::DSMService(const std::string& name): _name(name),
    _server(0),_subThreads(),_subThreadMutex(),_inputs(),
    _processors(),_ochans(),
    _threadPolicy(n_u::Thread::NU_THREAD_OTHER),_threadPriority(0)
{
}

DSMService::~DSMService()
{
    list<SampleInput*>::iterator li = _inputs.begin();
    for ( ; li != _inputs.end(); ++li) {
        SampleInput* input = *li;
        input->close();
	delete input;
    }
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = _processors.begin(); pi != _processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
	delete processor;
    }

    // The <output> sub-elements of <service> are not created
    // as SampleOutputs. IOChannels are created for
    // elements within the <output> element, like <socket>, <fileset>. etc.
    // So we can just delete the IOChannels.
    list<IOChannel*>::iterator oi = _ochans.begin();
    for ( ; oi != _ochans.end(); ++oi) {
        IOChannel* ochan = *oi;
        ochan->close();
        delete ochan;
    }
}

ProcessorIterator DSMService::getProcessorIterator() const
{
    return ProcessorIterator(this);
}

void DSMService::setDSMServer(DSMServer* val)
{
    _server = val;
}

void DSMService::addSubThread(n_u::Thread* thd) throw()
{
    n_u::Synchronized autolock(_subThreadMutex);
    _subThreads.insert(thd);
}

void DSMService::interrupt() throw()
{
    n_u::Synchronized autolock(_subThreadMutex);

    set<n_u::Thread*>::iterator si;
    for (si = _subThreads.begin(); si != _subThreads.end(); ++si) {
        n_u::Thread* thd = *si;
        try {
            if (thd->isRunning()) thd->interrupt();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            thd->getName().c_str(),e.what());
        }
    }
}

void DSMService::cancel() throw()
{
    n_u::Synchronized autolock(_subThreadMutex);

    set<n_u::Thread*>::iterator si;
    for (si = _subThreads.begin(); si != _subThreads.end(); ++si) {
        n_u::Thread* thd = *si;
        try {
            if (thd->isRunning()) thd->cancel();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            thd->getName().c_str(),e.what());
        }
    }
}

int DSMService::join() throw()
{
    n_u::Synchronized autolock(_subThreadMutex);

    set<n_u::Thread*>::iterator si;
    for (si = _subThreads.begin(); si != _subThreads.end(); ++si) {
        n_u::Thread* thd = *si;
        try {
            thd->join();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "service %s: %s",
            thd->getName().c_str(),e.what());
        }
	delete thd;
    }
    _subThreads.clear();
    return n_u::Thread::RUN_OK;
}

int DSMService::checkSubThreads() throw()
{
    n_u::Synchronized autolock(_subThreadMutex);

    int nrunning = 0;
    set<n_u::Thread*>::iterator si;
    for (si = _subThreads.begin(); si != _subThreads.end(); ) {
        n_u::Thread* thd = *si;
        if (!thd->isRunning()) {
            try {
                thd->join();
            }
            catch(const n_u::Exception& e) {
                n_u::Logger::getInstance()->log(LOG_ERR,
                        "thread %s has quit, exception=%s",
                thd->getName().c_str(),e.what());
            }
            delete thd;
            _subThreads.erase(si);
            si = _subThreads.begin();
        }
        else {
            nrunning++;
            ++si;
        }
    }
    return nrunning;
}

/* static */
const string DSMService::getClassName(const xercesc::DOMElement* node,
    const Project* project)
    throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& idref = xnode.getAttributeValue("IDREF");
    if (idref.length() > 0) {
	if (!project->getServiceCatalog())
	    throw n_u::InvalidParameterException(
		"service",
		"cannot find servicecatalog for service with IDREF",
		idref);

	const xercesc::DOMElement* cnode =
            project->getServiceCatalog()->find(idref);
	if (!cnode)
		throw n_u::InvalidParameterException(
	    "service",
	    "servicecatalog does not contain a service with ID",
	    idref);
	const string classattr = getClassName(cnode,project);
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
        const DSMServer* srvr = getDSMServer();
        assert(srvr);
	const Project* project = srvr->getProject();
        assert(project);
        if (!project->getServiceCatalog())
            throw n_u::InvalidParameterException(
                project->getName(),
                "cannot find servicecatalog for service with IDREF",
                idref);

        const xercesc::DOMElement* cnode =
            project->getServiceCatalog()->find(idref);
        if (!cnode)
                throw n_u::InvalidParameterException(
	    project->getName(),
            "servicecatalog does not contain a service with ID",
            idref);
        // read catalog entry
        fromDOMElement(cnode);
    }

    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            const string& aname = attr.getName();
            const string& aval = attr.getValue();
	    if (aname == "class" || aname == "ID" || aname == "IDREF");
            else if (aname == "priority") {
		string::size_type colon = aval.find(':',0);
		if (colon < string::npos) {
		    string policy = aval.substr(0,colon);
		    istringstream ist(aval.substr(colon+1));
		    int priority;
		    ist >> priority;
		    if (ist.fail())
			throw n_u::InvalidParameterException(
			    "DSMService::fromDOMElement, cannot read priority",
			    aname, aval);
                    _threadPriority = priority;
		    if (policy == "RT_FIFO") _threadPolicy = n_u::Thread::NU_THREAD_FIFO;
		    	// setRealTimeFIFOPriority(priority);
		    else if (policy == "RT_RR") _threadPolicy = n_u::Thread::NU_THREAD_RR;
		    	// setRealTimeRoundRobinPriority(priority);
		    else
			throw n_u::InvalidParameterException(
			    "DSMService::fromDOMElement, invalid priority (should be RT_FIFO, RT_RR or NICE",
			    aname, aval);
		}
	    }
            else if (aname == "rawSorterLength" || aname == "procSorterLength");
            else if (aname == "rawLateSampleCacheSize" || aname == "procLateSampleCacheSize");
            else if (aname == "rawHeapMax" || aname == "procHeapMax");
	    else throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"unrecognized attribute",aname);
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
        if (elname == "input") {
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
	    SampleInput* input =
                dynamic_cast<SampleInput*>(domable);
            if (!input) {
		delete domable;
                throw n_u::InvalidParameterException("service",
                    classattr,"is not a SampleInput");
	    }
            input->fromDOMElement((xercesc::DOMElement*)child);
            _inputs.push_back(input);
	}
        else if (elname == "processor") {
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
	    processor->setService(this);
            processor->fromDOMElement((xercesc::DOMElement*)child);
	    addProcessor(processor);
        }
        else if (elname == "output") {
            /* If a <service> has an <output> we don't create an object for it.
             * It just encloses an IOChannel, such as a  <socket>.  Therefore
             * we don't support a class attribute here.
             */
	    const string& classattr = xchild.getAttributeValue("class");
            if (classattr.length() > 0)
                throw n_u::InvalidParameterException(
                    "DSMService::fromDOMElement",
                    elname, "cannot have a class attribute");
            // parse all child elements of the output.
            xercesc::DOMNode* gkid;
            for (gkid = child->getFirstChild(); gkid != 0;
                    gkid=gkid->getNextSibling())
            {
                if (gkid->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

                IOChannel* ochan;
                ochan = IOChannel::createIOChannel((xercesc::DOMElement*)gkid);
                ochan->fromDOMElement((xercesc::DOMElement*)gkid);
                _ochans.push_back(ochan);
            }
        }
        else throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                elname, "unsupported element");
    }
}

