
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <RawSampleService.h>
#include <DOMObjectFactory.h>
#include <Aircraft.h>

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(RawSampleService)

RawSampleService::RawSampleService():
	DSMService("RawSampleService"),input(0)
{
}

/*
 * Copy constructor. We create a copy of the original configured
 * RawSampleService when an input has connected.
 */
RawSampleService::RawSampleService(const RawSampleService& x,
	SampleInputStream* newinput):
	DSMService((const DSMService&)x),input(newinput)
{
    // loop over x's processors
    const list<SampleIOProcessor*>& xprocs = x.getProcessors();
    list<SampleIOProcessor*>::const_iterator oi;
    for (oi = xprocs.begin(); oi != xprocs.end(); ++oi) {
        SampleIOProcessor* xproc = *oi;

	// We don't clone non-single DSM processors.
	if (xproc->singleDSM()) {
	    // clone processor
	    SampleIOProcessor* proc = xproc->clone();
	    addProcessor(proc);
	}
    }
}

RawSampleService::~RawSampleService()
{

    // cerr << "~RawSampleService, disconnecting processors" << endl;
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	// cerr << "~RawSampleService, disconnecting proc " << proc->getName() << endl;
	if (!proc->singleDSM()) proc->disconnect(&merger);
	else if (input) proc->disconnect(input);
    }

    if (input) {
	// cerr << "~RawSampleService, closing " << input->getName() << endl;
        input->close();
	delete input;
    }
#ifdef DEBUG
    cerr << "~RawSampleService, deleting processors" << endl;
#endif
    for (pi = processors.begin(); pi != processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
#ifdef DEBUG
	cerr << "~RawSampleService, deleting " <<
	    processor->getName() << endl;
#endif
	delete processor;
    }
    // cerr << "~RawSampleService done" << endl;
}


/*
 * Initial schedule request.
 */
void RawSampleService::schedule() throw(atdUtil::Exception)
{
    cerr << "RawSampleService::schedule, dsms.size=" <<
    	getDSMConfigs().size() << endl;

    merger.setDSMConfigs(getDSMConfigs());

    // Connect non-single DSM processors to merger
    list<SampleIOProcessor*>::const_iterator oi;
    for (oi = processors.begin(); oi != processors.end(); ++oi) {
        SampleIOProcessor* processor = *oi;
	if (!processor->singleDSM()) {
	    try {
		processor->connect(&merger);
	    }
	    catch(const atdUtil::IOException& ioe) {
		atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %s connect to %s: %s",
		    getName().c_str(),processor->getName().c_str(),
		    merger.getName().c_str(),ioe.what());
	    }
	}
    }
    input->requestConnection(this);
}

void RawSampleService::interrupt() throw()
{
    if (subServices.size() > 0) {
	cerr << "subServices.size()=" << subServices.size() <<
		" closing " << input->getName() << endl;
	input->close();
	merger.removeInput(input);
    }
    DSMService::interrupt();
}
      
/*
 * This method is called when a SampleInput is connected.
 * It will be called on only the original RawSampleService,
 * not on the clones. It may be called multiple times
 * as each DSM makes a connection.
 */
void RawSampleService::connected(SampleInput* newinput) throw()
{
    // Figure out what DSM it came from

    SampleInputStream* newstream = dynamic_cast<SampleInputStream*>(newinput);

    assert(newstream);

    atdUtil::Inet4Address remoteAddr = newstream->getRemoteInet4Address();
    const DSMConfig* dsm = getSite()->findDSM(remoteAddr);

    if (!dsm) {
	atdUtil::Logger::getInstance()->log(LOG_WARNING,
	    "RawSampleService: connection from %s does not match an address of any. Ignoring connection.",
		remoteAddr.getHostAddress().c_str());
	return;
    }

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s (%s) has connected to %s",
	newstream->getName().c_str(),dsm->getName().c_str(),
	getName().c_str());

    newstream->addDSMConfig(dsm);
	    
    // If this is a new input make a copy of myself.
    // The copy will own the input.
    if (newstream != input) {
	cerr << newstream->getName() << " is a diff input from " <<
		input->getName() << endl;
	RawSampleService* newserv = new RawSampleService(*this,newstream);
	newserv->start();
	addSubService(newserv);
    }
    // merger does not own newstream. It just adds sample clients to it.
    merger.addInput(newstream);
}

/*
 * This method is called when a SampleInput is disconnected
 * (likely a DSM went down).  It will be called on the original
 * RawSampleService (whoever did the requestConnection).
 * The run() method of the service should have received an exception,
 * and so things may clean up by themselves, but we do a thread
 * cancel here to make sure.
 */
void RawSampleService::disconnected(SampleInput* inputx) throw()
{

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	inputx->getName().c_str(),getName().c_str());

#ifdef DEBUG
    cerr << "RawSampleService::disconnected, inputx=" << inputx <<
    	" input=" << input << endl;

    // Figure out what DSM it came from. Not necessary, just for info.
    atdUtil::Inet4Address remoteAddr = inputx->getRemoteInet4Address();
    const DSMConfig* dsm = getSite()->findDSM(remoteAddr);

    cerr << "RawSampleService::disconnected, dsm=" << dsm << 
    	" getDSMConfig()=" << getDSMConfig() << endl;
#endif

    // figure out the cloned service for the input.
    // One could use a map of RawSampleService* by SampleInput*
    // but that must must stay in sync with the subServices set.
    // To make things easier, we do a brute force search 
    // of the subServices set - it is small after all.

    atdUtil::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    RawSampleService* service = 0;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        RawSampleService* svc = (RawSampleService*) *si;
	if (svc->input == inputx) {
	    service = svc;
	    break;
	}
    }

    if (!service) {
	atdUtil::Logger::getInstance()->log(LOG_WARNING,
	    "%s: can't find service for input %s",
		getName().c_str(),inputx->getName().c_str());
	return;
    }

    merger.removeInput(inputx);

    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = service->getProcessors().begin();
    	pi != service->getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	proc->disconnect(inputx);
    }

    // non-single DSM processors
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	if (!proc->singleDSM()) proc->disconnect(&merger);
    }

    try {
	if (service->isRunning()) service->cancel();
    }
    catch(const atdUtil::Exception& e) {
	atdUtil::Logger::getInstance()->log(LOG_WARNING,
	    "Error cancelling service %s: %s",
		service->getName().c_str(),e.what());
    }
}

int RawSampleService::run() throw(atdUtil::Exception)
{
    input->init();		// throws atdUtil::IOException

    // connect processors to the input.  These are
    // single DSM processors, because this is a clone
    // of the original RawSampleService. The processor
    // then request connections to its outputs.
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = processors.begin(); pi != processors.end();
    	++pi) {
        SampleIOProcessor* processor = *pi;
	try {
	    processor->connect(input);
	}
	catch(const atdUtil::IOException& ioe) {
	    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		"%s: connecting %s to %s: %s",
		getName().c_str(),
		input->getName().c_str(),
		processor->getName().c_str(),
		ioe.what());
	}
    }

    // Process the input samples.
    try {
	for (;;) {
	    if (isInterrupted()) break;
	    input->readSamples();
	}
    }
    catch(const atdUtil::EOFException& e) {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "%s: %s: %s",
                getName().c_str(),input->getName().c_str(),e.what());
    }
    catch(const atdUtil::IOException& e) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
                getName().c_str(),input->getName().c_str(),e.what());
    }

    try {
	list<SampleIOProcessor*>::const_iterator pi;
	for (pi = processors.begin(); pi != processors.end(); ++pi) {
	    SampleIOProcessor* processor = *pi;
	    cerr << getName() << " calling disconnect of " << input->getName() <<
	    	" on " << processor->getName() << endl;
	    processor->disconnect(input);
	}
	cerr << "closing " << input->getName() << endl;
	input->close();
    }
    catch(const atdUtil::IOException& e) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
		getName().c_str(),input->getName().c_str(),e.what());
    }

    return 0;
}

/*
 * process <service class="RawSampleService"> element
 */
void RawSampleService::fromDOMElement(const DOMElement* node)
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
    xercesc::DOMNode* child;
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
		    "RawSampleService::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const atdUtil::Exception& e) {
                throw atdUtil::InvalidParameterException("service",
                    classattr,e.what());
            }
	    input = dynamic_cast<RawSampleInputStream*>(domable);
            if (!input) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
                    classattr,"is not a RawSampleInputStream");
	    }
            input->fromDOMElement((DOMElement*)child);
	}
        else if (!elname.compare("processor")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		    "RawSampleService::fromDOMElement",
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
	    // processor->setDSMService(this);
            processor->fromDOMElement((DOMElement*)child);
	    addProcessor(processor);
        }
        else throw atdUtil::InvalidParameterException(
                "RawSampleService::fromDOMElement",
                elname, "unsupported element");
    }
    if (!input)
        throw atdUtil::InvalidParameterException(
                "RawSampleService::fromDOMElement",
                "input", "no inputs specified");
    if (processors.size() == 0)
        throw atdUtil::InvalidParameterException(
                "RawSampleService::fromDOMElement",
                "processor", "no processors specified");
}

DOMElement* RawSampleService::toDOMParent(
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

DOMElement* RawSampleService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

