/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleIOProcessor::SampleIOProcessor(bool rawSource): _source(rawSource),
    _optional(false),_service(0)
{
}

// #define DEBUG
SampleIOProcessor::~SampleIOProcessor()
{
    list<SampleOutput*>::const_iterator oi = _origOutputs.begin();
    for ( ; oi != _origOutputs.end(); ++oi) {
        SampleOutput* output = *oi;

#ifdef DEBUG
	cerr << "~SampleIOProcessor, deleting orig output=" <<
		output->getName() << endl;
#endif
	output->finish();
	output->close();
	delete output;
    }

    list<SampleTag*>::const_iterator ti = _requestedTags.begin();
    for ( ; ti != _requestedTags.end(); ++ti)
	delete *ti;

    list<Parameter*>::const_iterator pi = _parameters.begin();
    for ( ; pi != _parameters.end(); ++pi) delete *pi;
}

void SampleIOProcessor::addRequestedSampleTag(SampleTag* tag)
	throw(nidas::util::InvalidParameterException)
{
    n_u::Autolock autolock(_tagsMutex);
    if (find(_requestedTags.begin(),_requestedTags.end(),tag) ==
        _requestedTags.end()) {
        _requestedTags.push_back(tag);
        _constRequestedTags.push_back(tag);
    }
}

std::list<const SampleTag*> SampleIOProcessor::getRequestedSampleTags() const
{
    n_u::Autolock alock(_tagsMutex);
    return _constRequestedTags;
}

void SampleIOProcessor::addSampleTag(const SampleTag* tag) throw()
{
    _source.addSampleTag(tag);
}

void SampleIOProcessor::removeSampleTag(const SampleTag* tag) throw()
{
    _source.removeSampleTag(tag);
}

void SampleIOProcessor::addParameter(Parameter* val)
	throw(n_u::InvalidParameterException)
{
    _parameters.push_back(val);
    _constParameters.push_back(val);
}

const std::string& SampleIOProcessor::getName() const { return _name; }

void SampleIOProcessor::setName(const std::string& val) { _name = val; }

/*
 * process <processor> element
 */
void SampleIOProcessor::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    string className = xnode.getAttributeValue("class");
    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
            // get attribute name
           if (aname == "id") {
               istringstream ist(aval);
               // If you unset the dec flag, then a leading '0' means
               // octal, and 0x means hex.
               ist.unsetf(ios::dec);
               unsigned int val;
               ist >> val;
               if (ist.fail())
                   throw n_u::InvalidParameterException("sensor",
                       aname,aval);
               setSampleId(val);
           }
           else if (aname == "optional") {
               istringstream ist(aval);
		bool val;
		ist >> boolalpha >> val;
		if (ist.fail()) {
		    ist.clear();
		    ist >> noboolalpha >> val;
		    if (ist.fail())
			throw n_u::InvalidParameterException(
				"SampleIOProcessor", aname,aval);
		}
		setOptional(val);
	    }
        }
    }

    // process <output> and <sample> child elements
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	DOMable* domable;
        if (elname == "output") {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw n_u::InvalidParameterException(
		    "SampleIOProcessor::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const n_u::Exception& e) {
                throw n_u::InvalidParameterException("service",
                    classattr,e.what());
            }
	    SampleOutput* output = dynamic_cast<SampleOutput*>(domable);
            if (!output) {
		delete domable;
                throw n_u::InvalidParameterException(elname,
                    classattr,"is not a SampleOutput");
	    }
            output->fromDOMElement((xercesc::DOMElement*)child);
	    addOutput(output);
	}
	else if (elname == "sample") {
	    SampleTag* stag = new SampleTag();
	    stag->fromDOMElement((xercesc::DOMElement*)child);
	    stag->setSensorId(getId());
	    if (stag->getSampleId() == 0)
	        stag->setSampleId(getRequestedSampleTags().size()+1);
	    addRequestedSampleTag(stag);
	}
	else if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
        else throw n_u::InvalidParameterException(
                className + " SampleIOProcessor::fromDOMElement",
                elname, "unsupported element");
    }
    if (_origOutputs.size() == 0)
        throw n_u::InvalidParameterException(
                className + " SampleIOProcessor::fromDOMElement",
                "output", "no output specified");

}

