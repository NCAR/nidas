
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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
RawSampleService::RawSampleService(const RawSampleService& x):
	DSMService((const DSMService&)x),input(0)
{
    if (x.input) input = x.input->clone();

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
/*
 * clone myself by invoking copy constructor.
 */
DSMService* RawSampleService::clone() const
{
    return new RawSampleService(*this);
}

RawSampleService::~RawSampleService()
{

    list<SampleIOProcessor*>::const_iterator pi;
    if (input) {
#ifdef DEBUG
	cerr << "~RawSampleService, disconnecting processors, size()=" <<
	    processors.size() << endl;
#endif
	for (pi = processors.begin(); pi != processors.end(); ++pi) {
	    SampleIOProcessor* processor = *pi;
#ifdef DEBUG
	    cerr << "~RawSampleService, disconnecting " <<
	    	processor->getName() << " from " << input->getName() << endl;
#endif
	    processor->disconnect(input);
	}

#ifdef DEBUG
	cerr << "~RawSampleService, closing input" << endl;
#endif
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
}


/*
 * Initial schedule request.
 */
void RawSampleService::schedule() throw(atdUtil::Exception)
{
    input->requestConnection(this);
}

/*
 * This method is called when a SampleInput is connected.
 * It will be called on only the original RawSampleService,
 * not on the clones. It may be called multiple times
 * as each DSM makes a connection.
 */
void RawSampleService::connected(SampleInput* inpt) throw()
{

    assert(inpt == input);

    // Figure out what DSM it came from
    atdUtil::Inet4Address remoteAddr = input->getRemoteInet4Address();
    const DSMConfig* dsm = getAircraft()->findDSM(remoteAddr);

    if (!dsm)
	throw atdUtil::Exception(string("can't find DSM for address ") +
		remoteAddr.getHostAddress());

    input->setDSMConfig(dsm);

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s (%s) has connected to %s",
	input->getName().c_str(),dsm->getName().c_str(),
	getName().c_str());

    // make a copy of myself.
    RawSampleService* newserv = new RawSampleService(*this);
    newserv->start();

    addSubService(newserv);

    // Connect non-single DSM processors to newserv->input
    list<SampleIOProcessor*>::const_iterator oi;
    for (oi = processors.begin(); oi != processors.end(); ++oi) {
        SampleIOProcessor* processor = *oi;
	if (!processor->singleDSM()) {
	    try {
		processor->connect(newserv->input);
	    }
	    catch(const atdUtil::IOException& ioe) {
		atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %s connect to %s: %s",
		    getName().c_str(),processor->getName().c_str(),
		    newserv->input->getName().c_str(),ioe.what());
	    }
	}
    }
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
#endif

    // Figure out what DSM it came from. Probably not necessary
    atdUtil::Inet4Address remoteAddr = inputx->getRemoteInet4Address();
    const DSMConfig* dsm = getAircraft()->findDSM(remoteAddr);

#ifdef DEBUG
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

    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = service->getProcessors().begin();
    	pi != service->getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	proc->disconnect(inputx);
    }

    // non-single DSM processors
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	if (!proc->singleDSM()) proc->disconnect(inputx);
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

    input->init();

    // request connections for processors. These are all
    // single DSM processors, because this is a clone
    // of the original RawSampleService
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = processors.begin(); pi != processors.end();
    	++pi) {
        SampleIOProcessor* processor = *pi;
	try {
	    processor->connect(input);
	}
	catch(const atdUtil::IOException& ioe) {
	    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		"%s: requestConnection: %s: %s",
		getName().c_str(),processor->getName().c_str(),ioe.what());
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
	    processor->disconnect(input);
	}
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
	    input = dynamic_cast<SampleInput*>(domable);
            if (!input || !input->isRaw()) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
                    classattr,"is not a raw SampleInput");
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
                throw atdUtil::InvalidParameterException("service",
                    classattr,e.what());
            }
	    SampleIOProcessor* processor = dynamic_cast<SampleIOProcessor*>(domable);
            if (!processor) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
                    classattr,"is not of type SampleIOProcessor");
	    }
	    processor->setDSMService(this);
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

