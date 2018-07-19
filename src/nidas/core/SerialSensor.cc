// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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

#include "SerialSensor.h"
#include "TCPSocketIODevice.h"
#include "UDPSocketIODevice.h"
#include "BluetoothRFCommSocketIODevice.h"
#include "Looper.h"
#include "Prompt.h"

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <locale>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SerialSensor::SerialSensor():
    _workingPortConfig(), _serialDevice(0), _prompters(), _prompting(false)
{
    setDefaultMode(O_RDWR);
    _workingPortConfig.termios.setRaw(true);
    _workingPortConfig.termios.setRawLength(1);
    _workingPortConfig.termios.setRawTimeout(0);
}

SerialSensor::~SerialSensor()
{
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;
}

SampleScanner* SerialSensor::buildSampleScanner()
	throw(n_u::InvalidParameterException)
{
    SampleScanner* scanr = CharacterSensor::buildSampleScanner();
    DLOG(("%s: usec/byte=%d",getName().c_str(),getUsecsPerByte()));
    scanr->setUsecsPerByte(getUsecsPerByte());
    return scanr;
}

IODevice* SerialSensor::buildIODevice() throw(n_u::IOException)
{
    IODevice* device = CharacterSensor::buildIODevice();

    // Did we get the default from Character Sensor?
    if (reinterpret_cast<UnixIODevice*>(device)) {
        // yes, meaning it didn't look for, nor find a serial port.
        delete device;
        device = 0;
        DLOG(("SerialSensor: Instantiating a SerialPortIODevice on device ") << getDeviceName() 
            << "; Port Type: " << _workingPortConfig.xcvrConfig.portType);
        SerialPortIODevice* spDevice = new SerialPortIODevice(getDeviceName(), _workingPortConfig);
        _serialDevice = spDevice;
        return spDevice;
    }

    return device;
}

int SerialSensor::getUsecsPerByte() const
{
    if (_serialDevice) return _serialDevice->getUsecsPerByte();
    return 0;
}

void SerialSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    flags |= O_NOCTTY;
    CharacterSensor::open(flags);

    // Flush the serial port
    if (::isatty(getReadFd())) {
        int accmode = flags & O_ACCMODE;
        int fres;
        if (accmode == O_RDONLY) fres = ::tcflush(getReadFd(),TCIFLUSH);
        else if (accmode == O_WRONLY) fres = ::tcflush(getWriteFd(),TCOFLUSH);
        else fres = ::tcflush(getReadFd(),TCIOFLUSH);
        if (fres < 0) throw n_u::IOException(getName(),"tcflush",errno);
    }

    sendInitString();

    // FSSPs set up their message parameters in the sendInitString() method, so we can't check
    // for this problem until now.
    if (getDeviceName().find("usock:") != 0 &&
        getMessageLength() + getMessageSeparator().length() == 0)
            throw n_u::InvalidParameterException(getName(),"message","must specify a message separator or a non-zero message length");

    initPrompting();
}

// void SerialSensor::setMessageParameters(unsigned int len, const string& sep, bool eom)
//     throw(n_u::InvalidParameterException, n_u::IOException)
// {
//     CharacterSensor::setMessageParameters(len,sep,eom);

//     // Note we don't change _termios here.
//     // Termios is set to to raw mode, len=1, in the constructor.
//     // Very old NIDAS code did a _termio.setRawLength() to the
//     // message length, but not any more. I don't think it made
//     // things any more efficient, and may have reduced the accuracy of
//     // time-tagging.
// }

void SerialSensor::close() throw(n_u::IOException)
{
    shutdownPrompting();
    DSMSensor::close();
}

void SerialSensor::applyTermios() throw(nidas::util::IOException)
{
    DLOG(("SerialSensor::applyTermios(): entry"));
    if (_serialDevice) {
        _serialDevice->termios() = _workingPortConfig.termios;
        _serialDevice->applyTermios();
    }
    DLOG(("SerialSensor::applyTermios(): exit"));
}

void SerialSensor::applyPortConfig() 
{
    DLOG(("SerialSensor::applyPortConfig(): entry"));
    applyTermios();
    
    if (_serialDevice && _serialDevice->getXcvrCtrl()) {
        _serialDevice->setPortConfig(_workingPortConfig);
        _serialDevice->applyPortConfig();
    }
    DLOG(("SerialSensor::applyPortConfig(): exit"));
}

void SerialSensor::initPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
        const list<Prompt>& prompts = getPrompts();
        list<Prompt>::const_iterator pi = prompts.begin();
        for (; pi != prompts.end(); ++pi) {
           const Prompt& prompt = *pi;
           Prompter* prompter = new Prompter(this);
           prompter->setPrompt(n_u::replaceBackslashSequences(prompt.getString()));
           prompter->setPromptPeriodMsec((int) rint(MSECS_PER_SEC / prompt.getRate()));
           prompter->setPromptOffsetMsec((int) rint(MSECS_PER_SEC * prompt.getOffset()));

           _prompters.push_back(prompter);
           //addPrompter(n_u::replaceBackslashSequences(pi->getString()), (int) rint(MSECS_PER_SEC / pi->getRate()));
           // cerr << "promptPeriodMsec=" << _promptPeriodMsec << endl;
        }
        startPrompting();
    }
}

void SerialSensor::shutdownPrompting() throw(n_u::IOException)
{
    stopPrompting();
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;
    _prompters.clear();
}

void SerialSensor::startPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
        list<Prompter*>::const_iterator pi;
        //for (pi = getPrompters().begin(); pi != getPrompters.end(); ++pi) {
        for (pi = _prompters.begin(); pi != _prompters.end(); ++pi) {
            Prompter* prompter = *pi;
            getLooper()->addClient(prompter,
                    prompter->getPromptPeriodMsec(),
                    prompter->getPromptOffsetMsec());
        }
	_prompting = true;
    }
}

void SerialSensor::stopPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
        list<Prompter*>::const_iterator pi;
        //for (pi = getPrompters().begin(); pi = getPrompters().end(); ++pi) {
        for (pi = _prompters.begin(); pi != _prompters.end(); ++pi) {
            Prompter* prompter = *pi;
            getLooper()->removeClient(prompter);
        }
	_prompting = false;
    }
}

void SerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    try {
	ostr << "<td align=left>" << _workingPortConfig.termios.getBaudRate() <<
		_workingPortConfig.termios.getParityString().substr(0,1) <<
		_workingPortConfig.termios.getDataBits() << _workingPortConfig.termios.getStopBits();
	if (getReadFd() < 0) {
	    ostr << ",<font color=red><b>not active</b></font>";
	    if (getTimeoutMsecs() > 0)
	    	ostr << ",timeouts=" << getTimeoutCount();
	    ostr << "</td>" << endl;
	    return;
	}
	if (getTimeoutMsecs() > 0)
	    	ostr << ",timeouts=" << getTimeoutCount();
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
    }
}

void SerialSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    CharacterSensor::fromDOMElement(node);

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

	    if (aname == "ID");
	    else if (aname == "IDREF");
	    else if (aname == "class");
	    else if (aname == "devicename");
	    else if (aname == "id");
        else if (aname == "porttype") {
            string upperAval;
            std::transform(aval.begin(), aval.end(), upperAval.begin(), ::toupper);;
            if (upperAval == "RS232") _workingPortConfig.xcvrConfig.portType = RS232;
            else if (upperAval == "RS422") _workingPortConfig.xcvrConfig.portType = RS422;
            else if (upperAval == "RS485_HALF") _workingPortConfig.xcvrConfig.portType = RS485_HALF;
            else if (upperAval == "RS485_FULL") _workingPortConfig.xcvrConfig.portType = RS485_FULL;
            else throw n_u::InvalidParameterException(
                        string("SerialSensor:") + getName(),
                        aname,aval);
        }
        else if (aname == "termination") {
            if (aval == "NO_TERM") _workingPortConfig.xcvrConfig.termination = NO_TERM;
            else if (aval == "TERM_120_OHM") _workingPortConfig.xcvrConfig.termination = TERM_120_OHM;
            else throw n_u::InvalidParameterException(
                        string("SerialSensor:") + getName(),
                        aname,aval);
        }
	    else if (aname == "baud") {
            istringstream ist(aval);
            int val;
            ist >> val;
            if (ist.fail() || !_workingPortConfig.termios.setBaudRate(val))
                throw n_u::InvalidParameterException(
                    string("SerialSensor:") + getName(), aname,aval);
	    }
	    else if (aname == "parity") {
		if (aval == "odd") _workingPortConfig.termios.setParity(n_u::Termios::ODD);
		else if (aval == "even") _workingPortConfig.termios.setParity(n_u::Termios::EVEN);
		else if (aval == "none") _workingPortConfig.termios.setParity(n_u::Termios::NONE);
		else throw n_u::InvalidParameterException(
		    string("SerialSensor:") + getName(),
		    aname,aval);
	    }
	    else if (aname == "databits") {
            istringstream ist(aval);
            int val;
            ist >> val;
            if (ist.fail())
                throw n_u::InvalidParameterException(
                string("SerialSensor:") + getName(),
                    aname, aval);
            _workingPortConfig.termios.setDataBits(val);
	    }
	    else if (aname == "stopbits") {
            istringstream ist(aval);
            int val;
            ist >> val;
            if (ist.fail())
                throw n_u::InvalidParameterException(
                string("SerialSensor:") + getName(),
                    aname, aval);
            _workingPortConfig.termios.setStopBits(val);
	    }
	    else if (aname == "rts485") {
            if (aval == "true" || aval == "1") {
                _workingPortConfig.rts485 = 1;
            }
            else if (aval == "false" || aval == "0") {
                _workingPortConfig.rts485 = 0;
            }
            else if (aval == "-1") {
                _workingPortConfig.rts485 = -1;
            }
            else {
                throw n_u::InvalidParameterException(
                string("SerialSensor:") + getName(),
                    aname, aval);
            }
        }
	    else if (aname == "nullterm");
	    else if (aname == "init_string");
	    else if (aname == "suffix");
	    else if (aname == "height");
	    else if (aname == "depth");
	    else if (aname == "duplicateIdOK");
	    else if (aname == "timeout");
	    else if (aname == "readonly");
	    else if (aname == "station");
            else if (aname == "xml:base" || aname == "xmlns") {}
	    else throw n_u::InvalidParameterException(
		string("SerialSensor:") + getName(),
		"unknown attribute",aname);

	}
    }

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (elname == "message");
	else if (elname == "prompt");
	else if (elname == "sample");
	else if (elname == "parameter");
	else if (elname == "calfile");
	else throw n_u::InvalidParameterException(
	    string("SerialSensor:") + getName(),
	    "unknown element",elname);
    }
}

SerialSensor::Prompter::~Prompter()
{
    delete [] _prompt;
}

void SerialSensor::Prompter::setPrompt(const string& val)
{
    delete [] _prompt;
    _promptLen = val.length();
    _prompt = new char[_promptLen+1];
    // Use memcpy, not strcpy since the prompt may contain
    // null chars.
    memcpy(_prompt,val.c_str(),_promptLen);
}

void SerialSensor::Prompter::setPromptPeriodMsec(const int val)
{
    _promptPeriodMsec = val;
}

void SerialSensor::Prompter::setPromptOffsetMsec(const int val)
{
    _promptOffsetMsec = val;
}

void SerialSensor::Prompter::looperNotify() throw()
{
    if (!_prompt) return;
    try {
	_sensor->write(_prompt,_promptLen);
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: write prompt: %s",_sensor->getName().c_str(),
	    e.what());
    }
}

