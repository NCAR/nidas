// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>   // HAVE_BLUETOOTH_RFCOMM_H

#include "CharacterSensor.h"
#include "AsciiSscanf.h"
#include "IODevice.h"
#include "TCPSocketIODevice.h"
#include "UDPSocketIODevice.h"
#include "BluetoothRFCommSocketIODevice.h"
#include "Variable.h"
#include "Parameter.h"
#include "UnixIODevice.h"
#include "TimetagAdjuster.h"

#include <nidas/util/Logger.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

CharacterSensor::CharacterSensor():
    _ttadjusters(),
    _messageSeparator(),
    _separatorAtEOM(true),
    _messageLength(0),
    _prompts(),
    _sscanfers(),
    _nextSscanfer(),
    _maxScanfFields(0),
    _scanfFailures(0),
    _scanfPartials(0),
    _initString(),
    _emptyString()
{
    addPrompt(Prompt());
}

CharacterSensor::~CharacterSensor() {
    std::list<AsciiSscanf*>::iterator si;
    for (si = _sscanfers.begin(); si != _sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
	delete sscanf;
    }

    for (map<const SampleTag*, TimetagAdjuster*>::const_iterator tti =
            _ttadjusters.begin();
    	tti != _ttadjusters.end(); ++tti) delete tti->second;
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
 

void CharacterSensor::init() throw (n_u::InvalidParameterException)
{
    DLOG(("CharacterSensor::init() : Adding sscanfer's."));
    DSMSensor::init();

    const list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si = tags.begin();

    for (; si != tags.end(); ++si)
    {
        SampleTag* tag = *si;
        const string& sfmt = tag->getScanfFormat();
        if (sfmt.length() > 0)
        {
            AsciiSscanf* sscanf = new AsciiSscanf();
            try
            {
                sscanf->setFormat(n_u::replaceBackslashSequences(sfmt));
            } catch (n_u::ParseException& pe)
            {
                throw n_u::InvalidParameterException(getName(),
                        "setScanfFormat", pe.what());
            }

            int nd = 0;
            for (unsigned int iv = 0; iv < tag->getVariables().size(); iv++)
            {
                const Variable* var = tag->getVariables()[iv];
                nd += var->getLength();
            }

            sscanf->setSampleTag(tag);
            _sscanfers.push_back(sscanf);
            _maxScanfFields = std::max(
                    std::max(_maxScanfFields, sscanf->getNumberOfFields()), nd);
        }

        if (tag->getTimetagAdjustPeriod() > 0.0 && tag->getRate() > 0.0)
        {
            _ttadjusters[tag] =
                new TimetagAdjuster(tag->getRate(),
                    tag->getTimetagAdjustPeriod(),
                    tag->getTimetagAdjustSampleGap());
        }
    }

    if (!_sscanfers.empty())
        _nextSscanfer = _sscanfers.begin();
    else
        DLOG(("No sscanfer's added!!"));
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
    VLOG(("CharacterSensor::fromDOMElement(): entry..."));

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
    // the first prompt replaces the primary prompt.
    bool first_prompt = true;
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

            Prompt prompt;
            prompt.fromDOMElement((xercesc::DOMElement*)child);
            if (first_prompt)
            {
                setPrompt(prompt);
                first_prompt = false;
            }
            else
            {
                addPrompt(prompt);
            }
        }
    }

    VLOG(("CharacterSensor::fromDOMElement(): exit..."));
}


void CharacterSensor::validate() throw(nidas::util::InvalidParameterException)
{
    DSMSensor::validate();

    /* add prompts from the samples */
    const list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        SampleTag* samp = *si;
        // if the sample has a prompt string without a rate, addPrompt() will
        // inherit a non-zero rate from the primary prompt, so pass any
        // updates back to the sample tag.
        addPrompt(samp->getPrompt());
        samp->setPrompt(_prompts.back());
        // Samples without their own prompts or rates still inherit a rate
        // from a primary prompt, if any.
        if (samp->getRate() == 0)
        {
            samp->setRate(getPrompt().getRate());
        }
    }
}


int
CharacterSensor::
scanSample(AsciiSscanf* sscanf, const char* inputstr, float* data_ptr)
{
    return sscanf->sscanf(inputstr, data_ptr, sscanf->getNumberOfFields());
}



SampleT<float>*
CharacterSensor::
searchSampleScanners(const Sample* samp, SampleTag** stag_out) throw()
{
    // Note: sscanfers can be empty here, if a CharacterSensor was configured
    // with no samples, and hence no scanf strings.  For example,
    // a differential GPS, where nidas is supposed to take the
    // data for later use, but doesn't (currently) parse it.
    if (_sscanfers.empty())
    {
        DLOG(("No sscanfer's found!"));
        return 0;
    }
    assert(samp->getType() == CHAR_ST);

    const char* inputstr = (const char*)samp->getConstVoidDataPtr();
    int slen = samp->getDataByteLength();

    // if sample is not null terminated, create a new null-terminated sample
    if (inputstr[slen-1] != '\0')
    {
        SampleT<char>* newsamp = getSample<char>(slen+1);
        newsamp->setTimeTag(samp->getTimeTag());
        newsamp->setId(samp->getId());
        char* newstr = (char*)newsamp->getConstVoidDataPtr();
        ::memcpy(newstr,inputstr,slen);
        newstr[slen] = '\0';

        SampleT<float>* res = searchSampleScanners(newsamp, stag_out);
        newsamp->freeReference();
        return res;
    }

    SampleT<float>* outs = getSample<float>(_maxScanfFields);
    // Output sample always defaults to time of raw sample.
    outs->setTimeTag(samp->getTimeTag());
                 
    SampleTag* stag = 0;
    int nparsed = 0;
    unsigned int ntry = 0;
    AsciiSscanf* sscanf = 0;
    list<AsciiSscanf*>::const_iterator checkdone = _nextSscanfer;
    for ( ; ; ntry++)
    {
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

    if (!nparsed)
    {
        DLOG(("Nothing parsed: num failures: ") << _scanfFailures);
        _scanfFailures++;
        outs->freeReference();  // remember!
        return 0;               // no sample
    }

    // Fill and trim for unparsed values.
    trimUnparsed(stag, outs, nparsed);
    if (stag_out)
        *stag_out = stag;
    return outs;
}


bool
CharacterSensor::
process(const Sample* samp, list<const Sample*>& results) throw()
{
    // Try to scan the variables of a sample tag from the raw sensor
    // message.
    SampleTag* stag = 0;
    SampleT<float>* outs = searchSampleScanners(samp, &stag);
    if (!outs)
    {
        return false;
    }

    // Apply any time tag adjustments.
    adjustTimeTag(stag, outs);

    // Apply any variable conversions.  Note this has to happen after the
    // time is adjusted, since the calibrations are keyed by time.
    applyConversions(stag, outs);

    results.push_back(outs);
    return true;
}


void
CharacterSensor::
adjustTimeTag(SampleTag* stag, SampleT<float>* outs)
{                                 
    // If requested, reduce latency jitter in the time tags.
    // Then correct for a known sampling or sensor response lag.
    outs->setTimeTag(outs->getTimeTag() - getLagUsecs());
    TimetagAdjuster* ttadj = _ttadjusters[stag];
    if (ttadj)
    {
        outs->setTimeTag(ttadj->adjust(outs->getTimeTag()));
    }
}


void
CharacterSensor::addPrompt(const Prompt& prompt_in)
{
    Prompt prompt(prompt_in);
    if (prompt.hasPrompt() && prompt.getRate() == 0)
    {
        prompt.setRate(getPrompt().getRate());
    }
    _prompts.push_back(prompt);
    VLOG(("") << "pushed back prompt.  String = " << prompt.getString()
              << " rate= " << prompt.getRate());
}


void CharacterSensor::setPrompt(const Prompt& prompt)
{
    _prompts.front() = prompt;
}


const Prompt& CharacterSensor::getPrompt() const
{
    return _prompts.front();
}


const std::list<Prompt>& CharacterSensor::getPrompts() const
{
    return _prompts;
}


bool CharacterSensor::isPrompting() const
{
    return false;
}


bool CharacterSensor::isPrompted() const
{
    // see if any prompts are valid
    bool prompted = false;
    for (auto pi = _prompts.begin(); !prompted && pi != _prompts.end(); ++pi)
    {
        prompted = pi->valid();
    }
    return prompted;
}


void CharacterSensor::startPrompting() throw(nidas::util::IOException)
{
    throw nidas::util::IOException(getName(),
                                   "startPrompting","not supported");
}


void CharacterSensor::stopPrompting() throw(nidas::util::IOException)
{
    throw nidas::util::IOException(getName(),
                                   "stopPrompting","not supported");
}


void CharacterSensor::togglePrompting() throw(nidas::util::IOException)
{
    if (isPrompting())
        stopPrompting();
    else
        startPrompting();
}
