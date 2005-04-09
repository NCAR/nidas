/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SampleIOProcessor.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

SampleIOProcessor::SampleIOProcessor(): service(0) {}

SampleIOProcessor::SampleIOProcessor(const SampleIOProcessor& x):
	name(x.name),service(x.service)
{
#ifdef DEBUG
    cerr << "SampleIOProcessor copy ctor" << endl;
#endif
    list<SampleOutput*>::const_iterator oi;
    for (oi = x.outputs.begin(); oi != x.outputs.end(); ++oi) {
        SampleOutput* output = *oi;
        addOutput(output->clone());
    }
}


SampleIOProcessor::~SampleIOProcessor()
{
#ifdef DEBUG
    cerr << "~SampleIOProcessor, this=" << this << ", outputs.size=" << outputs.size() << endl;
#endif
    list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
#ifdef DEBUG
	cerr << "~SampleIOProcessor, output=" << output << endl;
#endif
	delete output;
    }
}

const std::string& SampleIOProcessor::getName() const { return name; }

void SampleIOProcessor::setName(const std::string& val) { name = val; }

void SampleIOProcessor::setDSMService(const DSMService* val)
{
    service = val;
    list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	output->setDSMService(val);
    }
}

const DSMService* SampleIOProcessor::getDSMService() const
{
    return service;
}

/*
 * process <processor> element
 */
void SampleIOProcessor::fromDOMElement(const DOMElement* node)
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

    // process <output> child elements
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	DOMable* domable;
        if (!elname.compare("output")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		    "SampleIOProcessor::fromDOMElement",
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
                    classattr,"is not a SampleOutput");
	    }
            output->fromDOMElement((DOMElement*)child);
	    addOutput(output);
	}
        else throw atdUtil::InvalidParameterException(
                "SampleIOProcessor::fromDOMElement",
                elname, "unsupported element");
    }
    if (outputs.size() == 0)
        throw atdUtil::InvalidParameterException(
                "SampleIOProcessor::fromDOMElement",
                "output", "no output specified");
}

DOMElement* SampleIOProcessor::toDOMParent(
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

DOMElement* SampleIOProcessor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

