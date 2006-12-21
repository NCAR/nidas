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

SampleIOProcessor::SampleIOProcessor(): id(0),optional(false),service(0)
{
}

/*
 * Copy constructor
 */

SampleIOProcessor::SampleIOProcessor(const SampleIOProcessor& x):
	name(x.name),id(x.id),optional(x.optional),service(x.service)
{
#ifdef DEBUG
    cerr << "SampleIOProcessor copy ctor" << endl;
#endif
    list<SampleOutput*>::const_iterator oi;
    for (oi = x.origOutputs.begin(); oi != x.origOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        addOutput(output->clone());
    }
}

// #define DEBUG
SampleIOProcessor::~SampleIOProcessor()
{
#ifdef DEBUG
    cerr << "~SampleIOProcessor, this=" << this <<
    	", origOutputs.size=" << origOutputs.size() << endl;
#endif

    outputMutex.lock();

    list<SampleOutput*>::const_iterator oi = pendingOutputClosures.begin();
    for (; oi != pendingOutputClosures.end(); ++oi) {
        SampleOutput* output = *oi;
	output->close();
	delete output;
    }
    pendingOutputClosures.clear();

    map<SampleOutput*,SampleOutput*>::const_iterator mi =
	outputMap.begin();
    for ( ; mi != outputMap.end(); ++mi) {
        SampleOutput* output = mi->first;
        SampleOutput* orig = mi->second;
	output->close();
	if (orig != output) {
#ifdef DEBUG
	    cerr << "~SampleIOProcessor, deleting output=" <<
	    	output->getName() << endl;
#endif
	    delete output;
	}
    }

    outputMutex.unlock();

    oi = origOutputs.begin();
    for ( ; oi != origOutputs.end(); ++oi) {
        SampleOutput* output = *oi;

#ifdef DEBUG
	cerr << "~SampleIOProcessor, deleting output=" <<
		output->getName() << endl;
#endif
	delete output;
    }

    set<SampleTag*>::const_iterator ti = sampleTags.begin();
    for ( ; ti != sampleTags.end(); ++ti)
	delete *ti;
}

void SampleIOProcessor::addSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{
    sampleTags.insert(tag);
    constSampleTags.insert(tag);
}


SampleTagIterator SampleIOProcessor::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator SampleIOProcessor::getVariableIterator() const
{
    return VariableIterator(this);
}

const std::string& SampleIOProcessor::getName() const { return name; }

void SampleIOProcessor::setName(const std::string& val) { name = val; }

void SampleIOProcessor::connect(SampleInput* input) throw(n_u::IOException)
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(), getName().c_str());

    list<SampleOutput*> tmpOutputs = origOutputs;
    list<SampleOutput*>::const_iterator oi;
    for (oi = tmpOutputs.begin(); oi != tmpOutputs.end(); ++oi) {
	SampleOutput* output = *oi;

	SampleTagIterator sti = getSampleTagIterator();
	for (; sti.hasNext(); ) output->addSampleTag(sti.next());

	output->requestConnection(this);
    }
}
 
void SampleIOProcessor::disconnect(SampleInput* input) throw(n_u::IOException)
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s is disconnecting from %s",
	input->getName().c_str(),getName().c_str());

    outputMutex.lock();
    map<SampleOutput*,SampleOutput*>::const_iterator mi =
	outputMap.begin();
    for ( ; mi != outputMap.end(); ++mi) {
        SampleOutput* output = mi->first;
        output->finish();
    }
    outputMutex.unlock();
}
 
void SampleIOProcessor::connected(SampleOutput* orig,SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s, #outputs=%d",
	output->getName().c_str(),
	getName().c_str(),
	outputMap.size());
    try {
	output->init();
    }
    catch( const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: error: %s",
	    output->getName().c_str(),ioe.what());
	disconnected(output);
	return;
    }
    outputMutex.lock();
    outputMap[output] = orig;
    outputSet.insert(output);

    list<SampleOutput*>::const_iterator oi = pendingOutputClosures.begin();
    for (; oi != pendingOutputClosures.end(); ++oi) {
        SampleOutput* output = *oi;
	delete output;
    }
    pendingOutputClosures.clear();
    outputMutex.unlock();
}
 
void SampleIOProcessor::disconnected(SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),
	getName().c_str());
    try {
	output->close();
    }
    catch (const n_u::IOException& ioe) {
        n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: error closing %s: %s",
	    getName().c_str(),output->getName().c_str(),ioe.what());
    }

    outputMutex.lock();
    SampleOutput* orig = outputMap[output];
    if (output != orig) pendingOutputClosures.push_back(output);

    outputMap.erase(output);
    outputSet.erase(output);

    outputMutex.unlock();

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
               unsigned long val;
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
        else throw n_u::InvalidParameterException(
                className + " SampleIOProcessor::fromDOMElement",
                elname, "unsupported element");
    }
    if (origOutputs.size() == 0)
        throw n_u::InvalidParameterException(
                className + " SampleIOProcessor::fromDOMElement",
                "output", "no output specified");

}

