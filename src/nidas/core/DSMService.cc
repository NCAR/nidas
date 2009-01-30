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

DSMService::DSMService(const std::string& name): _name(name),
	_server(0),_threadPolicy(n_u::Thread::NU_THREAD_OTHER),_threadPriority(0)
{
}

DSMService::~DSMService()
{
    list<SampleInputStream*>::iterator li = _inputs.begin();
    for ( ; li != _inputs.end(); ++li) {
        SampleInputStream* input = *li;
        input->close();
	delete input;
    }
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = _processors.begin(); pi != _processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
	delete processor;
    }
    list<IOChannel*>::iterator oi = _outputs.begin();
    for ( ; oi != _outputs.end(); ++oi) {
        IOChannel* output = *oi;
        output->close();
        delete output;
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

    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            const string& aname = attr.getName();
            const string& aval = attr.getValue();
	    if (aname == "priority") {
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
	    SampleInputStream* input =
                dynamic_cast<SampleInputStream*>(domable);
            if (!input) {
		delete domable;
                throw n_u::InvalidParameterException("service",
                    classattr,"is not a SampleInputStream");
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
        /* Processors often have their own <output>s,
         * but a <service>, like XMLConfigService may also have its
         * own <output>, which then typically doesn't have a <processor> */
        else if (elname == "output") {
            xercesc::DOMNode* gkid;
            // parse all child elements of the output.
            for (gkid = child->getFirstChild(); gkid != 0;
                    gkid=gkid->getNextSibling())
            {
                if (gkid->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

                IOChannel* output;
                output = IOChannel::createIOChannel((xercesc::DOMElement*)gkid);
                output->fromDOMElement((xercesc::DOMElement*)gkid);
                _outputs.push_back(output);
            }
        }
        else throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                elname, "unsupported element");
    }
    /*
    if (_inputs.size() == 0)
        throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                "input", "no inputs specified");
    if (_processors.size() == 0)
        throw n_u::InvalidParameterException(
                "DSMService::fromDOMElement",
                "processor", "no processors specified");
    */
}

