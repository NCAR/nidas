
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
#include <SocketAddress.h>

#include <DOMObjectFactory.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(RawSampleService)

RawSampleService::RawSampleService():
	DSMService("RawSampleService")
{
}

atdUtil::ServiceListenerClient* RawSampleService::clone()
{
    return new RawSampleService(*this);
}

int RawSampleService::run() throw(atdUtil::Exception)
{
    inputStream->setSocket(socket);

    for (;;) {
        inputStream->readSamples();
    }
    return 0;
}

void RawSampleService::fromDOMElement(const DOMElement* node)
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
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();
	cerr << "RawSampleService, child=" << elname << endl;

	if (!elname.compare("input")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		"RawSampleService::fromDOMElement",
		elname, "does not have a class attribute");
	    DOMable* input;
	    try {
		input = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("input",
		    classattr,e.what());
	    }
	    inputStream = dynamic_cast<SampleInputStream*>(input);
	    if (!inputStream) throw atdUtil::InvalidParameterException("input",
		    classattr,"is not a SampleInputStream");
	    inputStream->fromDOMElement((DOMElement*)child);
	}
	else if (!elname.compare("output")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		"RawSampleService::fromDOMElement",
		elname, "does not have a class attribute");
	    DOMable* output;
	    try {
		output = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("output",
		    classattr,e.what());
	    }
	    SampleOutputStream* sos =
		dynamic_cast<SampleOutputStream*>(output);
	    if (!sos) throw atdUtil::InvalidParameterException("output",
		    classattr,"is not a SampleOutputStream");
	    sos->fromDOMElement((DOMElement*)child);
	    outputStreams.push_back(sos);
	}
	else throw atdUtil::InvalidParameterException(
		"RawSampleService::fromDOMElement",
		elname, "unsupported element");
    }
    if (!inputStream)
	throw atdUtil::InvalidParameterException(
		"RawSampleService::fromDOMElement",
		"input", "no inputs specified");
    if (outputStreams.size() == 0)
	throw atdUtil::InvalidParameterException(
		"RawSampleService::fromDOMElement",
		"input", "no outputs specified");
    for (std::list<SampleOutputStream*>::iterator oi = outputStreams.begin();
    	oi != outputStreams.end(); ++oi)
	inputStream->addSampleClient(*oi);
    // call method of ServiceListenerClient base class so that
    // it knows the listen socket address
    setListenSocketAddress(inputStream->getSocketAddress());
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

