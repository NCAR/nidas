// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "SampleIOProcessor.h"
#include "NidsIterators.h"
#include "SampleOutput.h"
#include "SampleTag.h"
#include "Parameter.h"
#include "Project.h"
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleIOProcessor::SampleIOProcessor(bool rawSource): _source(rawSource),
    _tagsMutex(),_requestedTags(),_name(),_id(0),_constRequestedTags(),
    _origOutputs(), _optional(false),_service(0),_dsm(0),
    _parameters(),_constParameters()
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
	output->flush();
        try {
            output->close();
        }
        catch(const n_u::IOException&) {
        }
	delete output;
    }

    list<SampleTag*>::const_iterator ti = _requestedTags.begin();
    for ( ; ti != _requestedTags.end(); ++ti)
	delete *ti;

    list<Parameter*>::const_iterator pi = _parameters.begin();
    for ( ; pi != _parameters.end(); ++pi) delete *pi;
}

void SampleIOProcessor::addRequestedSampleTag(SampleTag* tag)
{
    n_u::Autolock autolock(_tagsMutex);
    if (find(_requestedTags.begin(),_requestedTags.end(),tag) ==
        _requestedTags.end()) {
        _requestedTags.push_back(tag);
        _constRequestedTags.push_back(tag);
    }
}

void SampleIOProcessor::removeRequestedSampleTag(SampleTag* tag)
{
    n_u::Autolock autolock(_tagsMutex);

    list<const SampleTag*>::iterator cti =
        find(_constRequestedTags.begin(),_constRequestedTags.end(),tag);

    list<SampleTag*>::iterator ti =
        find(_requestedTags.begin(),_requestedTags.end(),tag);

    assert((cti == _constRequestedTags.end()) ==
        (ti == _requestedTags.end()));

    if (cti != _constRequestedTags.end()) {
        _constRequestedTags.erase(cti);
        _requestedTags.erase(ti);
        delete tag;
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
            else if (aname == "xml:base" || aname == "xmlns") {}
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
                // If this is NetcdfRPCOutput, then likely it was not compiled
                // in and is not needed, so just warn and move on.
                if (classattr.find("NetcdfRPCOutput") != string::npos)
                {
                    WLOG(("Output class not found, skipping: ") << classattr);
                    continue;
                }
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
	    	Parameter::createParameter((xercesc::DOMElement*)child,&Project::getInstance()->getDictionary());
	    addParameter(parameter);
	}
        else throw n_u::InvalidParameterException(
                className + " SampleIOProcessor::fromDOMElement",
                elname, "unsupported element");
    }
    if (_origOutputs.empty())
    {
        // Warn about missing outputs, but do not make it fatal since the
        // outputs may not be required for this particular invocation.
        WLOG(("") << className << " SampleIOProcessor::fromDOMElement :"
                  << "no output specified");
    }
}

