/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <DSMService.h>
#include <DOMObjectFactory.h>

using namespace dsm;
using namespace std;

void DSMService::fromDOMElement(const xercesc::DOMElement* node)
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
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	cerr << "DSMService, child=" << elname << endl;

	if (!elname.compare("input")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		"DSMService::fromDOMElement",
		elname, "does not have a class attribute");
	    DOMable* input;
	    try {
		input = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("input",
		    classattr,e.what());
	    }
	    SampleInputStream* sis = dynamic_cast<SampleInputStream*>(input);
	    if (!sis) throw atdUtil::InvalidParameterException("input",
		    classattr,"is not a SampleInputStream");
	    sis->fromDOMElement((xercesc::DOMElement*)child);
	    inputStreams.push_back(sis);
	    cerr << "inputStreams.size=" << inputStreams.size() << endl;
	}
	else if (!elname.compare("output")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		"DSMService::fromDOMElement",
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
	    sos->fromDOMElement((xercesc::DOMElement*)child);
	    outputStreams.push_back(sos);
	}
	else throw atdUtil::InvalidParameterException(
		"DSMService::fromDOMElement",
		elname, "unsupported element");
    }
    if (inputStreams.size() == 0)
	throw atdUtil::InvalidParameterException(
		"DSMService::fromDOMElement",
		"input", "no inputs specified");
    if (outputStreams.size() == 0)
	throw atdUtil::InvalidParameterException(
		"DSMService::fromDOMElement",
		"input", "no outputs specified");
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

