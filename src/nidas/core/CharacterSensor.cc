// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/Config.h>   // HAVE_BLUETOOTH_RFCOMM_H

#include <nidas/core/CharacterSensor.h>
#include <nidas/core/AsciiSscanf.h>
#include <nidas/core/IODevice.h>
#include <nidas/core/TCPSocketIODevice.h>
#include <nidas/core/UDPSocketIODevice.h>
#include <nidas/core/BluetoothRFCommSocketIODevice.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

CharacterSensor::CharacterSensor():
    _messageSeparator(),
    _separatorAtEOM(true),
    _messageLength(0),
    _prompts(),
    _promptString(),
    _promptRate(0.0),
    _sscanfers(),
    _nextSscanfer(),
    _maxScanfFields(0),
    _scanfFailures(0),
    _scanfPartials(0),
    _prompted(false),
    _initString(),_emptyString()
{
}

CharacterSensor::~CharacterSensor() {
    std::list<AsciiSscanf*>::iterator si;
    for (si = _sscanfers.begin(); si != _sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
	delete sscanf;
    }
}

void CharacterSensor::setMessageParameters(unsigned int len, const std::string& sep, bool eom)
    throw(n_u::InvalidParameterException,n_u::IOException)
{
    if (sep.length() == 0 && len == 0)
        throw n_u::InvalidParameterException(getName(),"message","no message separator and message length equals 0");
    _messageSeparator = n_u::replaceBackslashSequences(sep);
    _separatorAtEOM = eom;
    _messageLength = len;
    if (getSampleScanner()) getSampleScanner()->setMessageParameters(len,_messageSeparator,eom);
}

void CharacterSensor::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    DSMSensor::open(flags);
    // Cannot sendInitString yet.
    // DSMSerialSensors are not yet fully initialized,
    // so it must be done in a derived class open(), i.e.
    // DSMSerialSensor.
}

void CharacterSensor::sendInitString() throw(n_u::IOException)
{
    if (getInitString().length() > 0) {
	DLOG(("sending init string '") << getInitString()
	     << "' to " << getDeviceName());
        string newstr = n_u::replaceBackslashSequences(getInitString());
        write(newstr.c_str(),newstr.length());
    }
}

IODevice* CharacterSensor::buildIODevice() throw(n_u::IOException)
{
    if (getDeviceName().find("inet:") == 0)
        return new TCPSocketIODevice();
    else if (getDeviceName().find("sock:") == 0)
        return new TCPSocketIODevice();
    else if (getDeviceName().find("usock:") == 0)
        return new UDPSocketIODevice();
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    else if (getDeviceName().find("btspp:") == 0)
        return new BluetoothRFCommSocketIODevice();
#endif
    else return new UnixIODevice();
}

SampleScanner* CharacterSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    SampleScanner* scanr;
    if (getDeviceName().find("usock:") == 0) {
        DatagramSampleScanner* dscanr;
        scanr = dscanr = new DatagramSampleScanner();
        dscanr->setNullTerminate(doesAsciiSscanfs());
    }
    else {
        MessageStreamScanner* mscanr;
        scanr = mscanr = new MessageStreamScanner();
        mscanr->setNullTerminate(doesAsciiSscanfs());

        scanr->setMessageParameters(getMessageLength(),
            getMessageSeparator(),getMessageSeparatorAtEOM());
    }
    return scanr;
}

bool CharacterSensor::doesAsciiSscanfs()
{
    for (SampleTagIterator si = getSampleTagIterator(); si.hasNext(); ) {
	const SampleTag* tag = si.next();
	const string& sfmt = tag->getScanfFormat();
	if (sfmt.length() > 0) return true;
    }
    return false;
}
 
void CharacterSensor::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    const list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si = tags.begin();

    for ( ; si != tags.end(); ++si) {
	SampleTag* tag = *si;
	const string& sfmt = tag->getScanfFormat();
	if (sfmt.length() > 0) {
	    AsciiSscanf* sscanf = new AsciiSscanf();
	    try {
	       sscanf->setFormat(n_u::replaceBackslashSequences(sfmt));
	    }
	    catch (n_u::ParseException& pe) {
		throw n_u::InvalidParameterException(getName(),
		       "setScanfFormat",pe.what());
	    }

            int nd = 0;
            for (unsigned int iv = 0; iv < tag->getVariables().size(); iv++) {
                const Variable* var = tag->getVariables()[iv];
                nd += var->getLength();
            }

	    sscanf->setSampleTag(tag);
	    _sscanfers.push_back(sscanf);
	    _maxScanfFields = std::max(std::max(_maxScanfFields,sscanf->getNumberOfFields()),nd);
	}
    }
	
    if (!_sscanfers.empty()) _nextSscanfer = _sscanfers.begin();
    validateSscanfs();
}

void CharacterSensor::validateSscanfs() throw(n_u::InvalidParameterException)
{
    /* default implementation */
    std::list<AsciiSscanf*>::const_iterator si = _sscanfers.begin();
    for ( ; si != _sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
        const SampleTag* tag = sscanf->getSampleTag();

        int nd = 0;
        for (unsigned int iv = 0; iv < tag->getVariables().size(); iv++) {
            const Variable* var = tag->getVariables()[iv];
            nd += var->getLength();
        }

        /* could turn this into an InvalidParameterException at some point */
        if (sscanf->getNumberOfFields() < nd)
            n_u::Logger::getInstance()->log(LOG_WARNING,
                "%s: number of scanf fields (%d) is less than the number of variable values (%d)",
                getName().c_str(),sscanf->getNumberOfFields(),nd);
    }
}

void CharacterSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    DSMSensor::fromDOMElement(node);

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

	    if (aname == "nullterm") { }
	    else if (aname == "init_string")
		setInitString(aval);

	}
    }
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (elname == "message") {
	    const string& str = xchild.getAttributeValue("position");
            bool eom = true;
	    if (str == "beg") eom = false;
	    else if (str == "end") eom = true;
	    else if (str != "") throw n_u::InvalidParameterException
			(getName(),"messageSeparator position",str);

	    istringstream ist(xchild.getAttributeValue("length"));
	    unsigned int len;
	    ist >> len;
	    if (ist.fail())
		throw n_u::InvalidParameterException(getName(),
		    "message length", xchild.getAttributeValue("length"));

            // The signature of this method indicates that it can throw IOException,
            // but it won't actually, since the device isn't opened yet.
            try {
                setMessageParameters(len,xchild.getAttributeValue("separator"),eom);
            }
            catch(const n_u::IOException& e) {
                throw n_u::InvalidParameterException(e.what());
            }
	}
	else if (elname == "prompt") {
	    std::string prompt = xchild.getAttributeValue("string");

	    setPromptString(prompt);

	    istringstream ist(xchild.getAttributeValue("rate"));
	    float rate;
	    ist >> rate;
	    if (ist.fail())
		throw n_u::InvalidParameterException(getName(),
		    "prompt rate", xchild.getAttributeValue("rate"));

	    if (rate < 0.0)
		throw n_u::InvalidParameterException
			(getName(),"prompt rate",
			    xchild.getAttributeValue("rate"));
            setPromptRate(rate);
            //addPrompt(prompt, rate);
	}
    }
}

void CharacterSensor::validate() throw(nidas::util::InvalidParameterException)
{
    DSMSensor::validate();

    if (!getPromptString().empty()) addPrompt(getPromptString(), getPromptRate());

    /* determine if any of the samples have associated prompts */
    const list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
	SampleTag* samp = *si;
	if (samp->getRate() == 0.0 && getPromptRate() > 0.0)
	    samp->setRate(getPromptRate());
	if (!samp->getPromptString().empty()) {
	    addPrompt(samp->getPromptString(), samp->getRate());
	    if (samp->getRate() <= 0.0)
	        throw n_u::InvalidParameterException(
		    getName() + " prompted sensor has sample rate <= 0.0");
        }
    }

    _prompted = !getPrompts().empty();
}


int
CharacterSensor::
scanSample(AsciiSscanf* sscanf, const char* inputstr, float* data_ptr)
{
    return sscanf->sscanf(inputstr, data_ptr, sscanf->getNumberOfFields());
}


bool CharacterSensor::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    // Note: sscanfers can be empty here, if a CharacterSensor was configured
    // with no samples, and hence no scanf strings.  For example,
    // a differential GPS, where nidas is supposed to take the
    // data for later use, but doesn't (currently) parse it.
    if (_sscanfers.empty()) return false;

    assert(samp->getType() == CHAR_ST);

    const char* inputstr = (const char*)samp->getConstVoidDataPtr();
    int slen = samp->getDataByteLength();

    // if sample is not null terminated, create a new null-terminated sample
    if (inputstr[slen-1] != '\0') {
        SampleT<char>* newsamp = getSample<char>(slen+1);
        newsamp->setTimeTag(samp->getTimeTag());
        newsamp->setId(samp->getId());
        char* newstr = (char*)newsamp->getConstVoidDataPtr();
        ::memcpy(newstr,inputstr,slen);
        newstr[slen] = '\0';

        bool res =  CharacterSensor::process(newsamp,results);
        newsamp->freeReference();
        return res;
    }

    SampleT<float>* outs = getSample<float>(_maxScanfFields);

    SampleTag* stag = 0;
    int nparsed = 0;
    unsigned int ntry = 0;
    AsciiSscanf* sscanf = 0;
    list<AsciiSscanf*>::const_iterator checkdone = _nextSscanfer;
    for ( ; ; ntry++) {
	sscanf = *_nextSscanfer;
	nparsed = scanSample(sscanf, inputstr, outs->getDataPtr());
	if (++_nextSscanfer == _sscanfers.end()) 
	    _nextSscanfer = _sscanfers.begin();
	if (nparsed > 0) {
	    stag = sscanf->getSampleTag();
	    outs->setId(stag->getId());
	    if (nparsed != sscanf->getNumberOfFields()) _scanfPartials++;
	    break;
	}
        if (_nextSscanfer == checkdone) break;
    }
    static n_u::LogContext lp(LOG_DEBUG);

    if (lp.active() && nparsed != sscanf->getNumberOfFields())
    {
	n_u::LogMessage msg;
	msg << (nparsed > 0 ? "partial" : "failed")
	    << " scanf; tried " << (ntry+(nparsed>0))
	    << "/" << _sscanfers.size() << " formats.\n";
	msg << "input:'" << inputstr << "'\n"
	    << "last format tried: " << (sscanf ? sscanf->getFormat() : "X")
	    << "\n";
	msg << "; nparsed=" << nparsed
	    << "; scanfFailures=" << _scanfFailures
	    << "; scanfPartials=" << _scanfPartials;
	lp.log(msg);
    }	

    if (!nparsed) {
	_scanfFailures++;
	outs->freeReference();	// remember!
	return false;		// no sample
    }

    float* fp = outs->getDataPtr();
    const vector<Variable*>& vars = stag->getVariables();
    int nd = 0;
    for (unsigned int iv = 0; iv < vars.size(); iv++) {
        Variable* var = vars[iv];
        for (unsigned int id = 0; id < var->getLength(); id++,nd++,fp++) {
            if (nd >= nparsed) *fp = floatNAN;  // this value not parsed
            else {
                float val = *fp;
                /* check for missing value before conversion. This
                 * is for sensors that put out something like -9999
                 * for a missing value, which should be checked before
                 * any conversion, and for which an exact equals check
                 * should work.  Doing a equals check on a numeric after a
                 * conversion is problematic.
                 */
                if (val == var->getMissingValue()) val = floatNAN;
                else {
                    if (getApplyVariableConversions()) {
                        VariableConverter* conv = var->getConverter();
                        if (conv) val = conv->convert(samp->getTimeTag(),val);
                    }

                    /* Screen values outside of min,max after the conversion */
                    if (val < var->getMinValue() || val > var->getMaxValue()) 
                        val = floatNAN;
                }
                *fp = val;
            }
        }
    }
    // correct for the sampling lag.
    outs->setTimeTag(samp->getTimeTag() - getLagUsecs());
    outs->setDataLength(nd);
    results.push_back(outs);
    return true;
}

