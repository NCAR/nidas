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
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleIOProcessor::SampleIOProcessor(): _id(0),_optional(false),_service(0)
{
}

#ifdef NEED_COPY_CLONE
/*
 * Copy constructor
 */
SampleIOProcessor::SampleIOProcessor(const SampleIOProcessor& x):
	_name(x._name),_id(x._id),_optional(x._optional),_service(x._service)
{
#ifdef DEBUG
    cerr << "SampleIOProcessor copy ctor" << endl;
#endif
    list<SampleOutput*>::const_iterator oi;
    for (oi = x._origOutputs.begin(); oi != x._origOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        addOutput(output->clone());
    }

    list<const Parameter*>::const_iterator pi = x._constParameters.begin();
    for ( ; pi != x._constParameters.end(); ++pi) {
        const Parameter* param = *pi;
        addParameter(param->clone());
    }
}
#endif

// #define DEBUG
SampleIOProcessor::~SampleIOProcessor()
{
#ifdef DEBUG
    cerr << "~SampleIOProcessor, this=" << this <<
    	", origOutputs.size=" << _origOutputs.size() << endl;
#endif

    _outputMutex.lock();

    list<SampleOutput*>::const_iterator oi = _pendingOutputClosures.begin();
    for (; oi != _pendingOutputClosures.end(); ++oi) {
        SampleOutput* output = *oi;
	output->finish();
	output->close();
	delete output;
    }
    _pendingOutputClosures.clear();

    map<SampleOutput*,SampleOutput*>::const_iterator mi =
	_outputMap.begin();
    for ( ; mi != _outputMap.end(); ++mi) {
        SampleOutput* output = mi->first;
        SampleOutput* orig = mi->second;
	if (orig != output) {
            output->finish();
            output->close();
#ifdef DEBUG
	    cerr << "~SampleIOProcessor, deleting non-orig output=" <<
	    	output->getName() << endl;
#endif
	    delete output;
	}
    }

    _outputMutex.unlock();

    oi = _origOutputs.begin();
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

    list<SampleTag*>::const_iterator ti = _sampleTags.begin();
    for ( ; ti != _sampleTags.end(); ++ti)
	delete *ti;

    list<Parameter*>::const_iterator pi = _parameters.begin();
    for ( ; pi != _parameters.end(); ++pi) delete *pi;

}

void SampleIOProcessor::addSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{
    if (find(_sampleTags.begin(),_sampleTags.end(),tag) == _sampleTags.end()) {
        _sampleTags.push_back(tag);
        _constSampleTags.push_back(tag);
    }
}

void SampleIOProcessor::addParameter(Parameter* val)
	throw(n_u::InvalidParameterException)
{
    _parameters.push_back(val);
    _constParameters.push_back(val);
}


SampleTagIterator SampleIOProcessor::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator SampleIOProcessor::getVariableIterator() const
{
    return VariableIterator(this);
}

const std::string& SampleIOProcessor::getName() const { return _name; }

void SampleIOProcessor::setName(const std::string& val) { _name = val; }

void SampleIOProcessor::connect(SampleInput* input) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(), getName().c_str());

    list<SampleOutput*> tmpOutputs = _origOutputs;
    list<SampleOutput*>::const_iterator oi;
    for (oi = tmpOutputs.begin(); oi != tmpOutputs.end(); ++oi) {
	SampleOutput* output = *oi;

	SampleTagIterator sti = getSampleTagIterator();
	for (; sti.hasNext(); ) output->addSampleTag(sti.next());
	output->requestConnection(this);
    }
}
 
void SampleIOProcessor::disconnect(SampleInput* input) throw()
{
    n_u::Logger::getInstance()->log(LOG_DEBUG,
	"%s is disconnecting from %s",
	input->getName().c_str(),getName().c_str());

    _outputMutex.lock();
    map<SampleOutput*,SampleOutput*>::const_iterator mi =
	_outputMap.begin();
    for ( ; mi != _outputMap.end(); ++mi) {
        SampleOutput* output = mi->first;
        output->finish();
    }
    _outputMutex.unlock();
}
 
void SampleIOProcessor::connect(SampleOutput* orig,SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s, #outputs=%d",
	output->getName().c_str(),getName().c_str(),
	_outputMap.size());
    try {
	output->init();
    }
    catch( const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,"%s: error: %s",
	    output->getName().c_str(),ioe.what());
	disconnect(output);
	return;
    }
    _outputMutex.lock();
    _outputMap[output] = orig;
    _outputSet.insert(output);

    list<SampleOutput*>::const_iterator oi = _pendingOutputClosures.begin();
    for (; oi != _pendingOutputClosures.end(); ++oi) {
        SampleOutput* output = *oi;
	delete output;
    }
    _pendingOutputClosures.clear();
    _outputMutex.unlock();
}
 
void SampleIOProcessor::disconnect(SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s is disconecting from %s",
	output->getName().c_str(),
	getName().c_str());
    try {
	output->finish();
	output->close();
    }
    catch (const n_u::IOException& ioe) {
        n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: error closing %s: %s",
	    getName().c_str(),output->getName().c_str(),ioe.what());
    }

    _outputMutex.lock();
    SampleOutput* orig = _outputMap[output];
    if (output != orig) _pendingOutputClosures.push_back(output);

    _outputMap.erase(output);
    _outputSet.erase(output);

    _outputMutex.unlock();

    // if (orig) orig->requestConnection(this);
}


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
               setShortId(val);
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
	    stag->setDSMId(getDSMId());
	    stag->setSensorId(getShortId());
	    if (stag->getSampleId() == 0)
	        stag->setSampleId(getSampleTags().size());
	    addSampleTag(stag);
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

