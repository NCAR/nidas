
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

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(RawSampleService)

RawSampleService::RawSampleService():
	DSMService("RawSampleService"),input(0)
{
}

/*
 * Copy constructor.
 */
RawSampleService::RawSampleService(const RawSampleService& x):
	DSMService((const DSMService&)x),input(0)
{
    if (x.input) input = new RawSampleInputStream(*x.input);

    // loop over x.outputs
    std::list<SampleOutput*>::const_iterator oi;
    for (oi = x.outputs.begin(); oi != x.outputs.end(); ++oi) {
        SampleOutput* output = *oi;

	// don't clone singleton outputs. They receive
	// samples from multiple DSMs.
	// Attach them to the new inputs though.
	// Only the original RawSampleService will have
	// a singleton output.
	if (output->isSingleton()) {
	    if (input) input->addSampleClient(output);
	}
	else {
	    // clone output
	    SampleOutput* newout = output->clone();
	    outputs.push_back(newout);
	}
    }
}
/*
 * clone myself
 */
DSMService* RawSampleService::clone() const
{
    // invoke copy constructor.
    RawSampleService* newserv = new RawSampleService(*this);
    return newserv;
}

RawSampleService::~RawSampleService()
{
    if (input) {
        input->close();
	delete input;
    }
    std::list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	output->close();
	delete output;
    }
}

/*
 * Initial schedule request.
 */
void RawSampleService::schedule() throw(atdUtil::Exception)
{
    input->requestConnection(this);
    std::list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	if (output->isSingleton()) output->requestConnection(this);
    }
}

/*
 * This method is called when a SampleInput is connected.
 */
void RawSampleService::connected(SampleInput* inpt)
{

    assert(inpt == input);
    // Figure out what DSM it came from
    atdUtil::Inet4Address remoteAddr = input->getRemoteInet4Address();
    const DSMConfig* dsm = getAircraft()->findDSM(remoteAddr);

    if (!dsm)
	throw atdUtil::Exception(string("can't find DSM for address ") +
		remoteAddr.getHostAddress());
    // make a copy of myself, assign it to a specific dsm
    RawSampleService* newserv = new RawSampleService(*this);
    newserv->setDSMConfig(dsm);
    newserv->start();
    getServer()->addThread(newserv);
}
/*
 * This method is called when a SampleOutput is connected.
 */
void RawSampleService::connected(SampleOutput* outpt)
{
    std::list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	if (output == outpt) {
	    output->init();
	    input->addSampleClient(output);
	}
    }
}

int RawSampleService::run() throw(atdUtil::Exception)
{
    input->init();

    std::list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	assert(!output->isSingleton());
	output->setDSMConfig(getDSMConfig());
	output->requestConnection(this);
    }

    // This is a simple service that just echoes the input
    // samples to the output

    try {
	for (;;) {
	    if (isInterrupted()) break;
	    input->readSamples();
	}
    }
    catch(const atdUtil::EOFException& e) {
	cerr << "RawSampleService " << getName() << ": " << e.what() << endl;
    }

    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	input->removeSampleClient(output);
	output->flush();
	output->close();
    }

    input->close();
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
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
        }
    }

    // process <input>, <output> child elements
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
        if (!elname.compare("input")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() > 0 &&
	    	classattr.compare("RawSampleInputStream"))
		throw atdUtil::InvalidParameterException(
		    "RawSampleService::fromDOMElement",
		    elname, "must be of class RawSampleInputStream");
	    input = new RawSampleInputStream();
	    input->fromDOMElement((xercesc::DOMElement*)child);
	}
        else if (!elname.compare("output")) {
	    DOMable* domable;
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
	    SampleOutput* output = dynamic_cast<SampleOutput*>(domable);
            if (!output) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
                    classattr,"is not of type DSMService");
	    }
            output->fromDOMElement((DOMElement*)child);
	    outputs.push_back(output);
        }
        else throw atdUtil::InvalidParameterException(
                "DSMService::fromDOMElement",
                elname, "unsupported element");
    }
    if (!input)
        throw atdUtil::InvalidParameterException(
                "DSMService::fromDOMElement",
                "input", "no inputs specified");
    if (outputs.size() == 0)
        throw atdUtil::InvalidParameterException(
                "DSMService::fromDOMElement",
                "output", "no outputs specified");
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

