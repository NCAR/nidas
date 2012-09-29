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

#include <nidas/core/SerialSensor.h>
#include <nidas/core/TCPSocketIODevice.h>
#include <nidas/core/UDPSocketIODevice.h>
#include <nidas/core/BluetoothRFCommSocketIODevice.h>
#include <nidas/core/Looper.h>
#include <nidas/core/Prompt.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SerialSensor::SerialSensor():
    _termios(),_serialDevice(0),_prompters(),_prompting(false)
{
    setDefaultMode(O_RDWR);
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
    if (getDeviceName().find("inet:") == 0)
        return new TCPSocketIODevice();
    else if (getDeviceName().find("sock:") == 0)
        return new TCPSocketIODevice();
    else if (getDeviceName().find("usock:") == 0)
        return new UDPSocketIODevice();
#ifdef HAS_BLUETOOTHRFCOMM_H
    else if (getDeviceName().find("btspp:") == 0)
        return new BluetoothRFCommSocketIODevice();
#endif
    else {
        _serialDevice = new SerialPortIODevice();
        _serialDevice->termios() = _termios;
        return _serialDevice;
    }
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

    initPrompting();
}

void SerialSensor::setMessageParameters(unsigned int len, const string& sep, bool eom)
    throw(n_u::InvalidParameterException, n_u::IOException)
{
    CharacterSensor::setMessageParameters(len,sep,eom);
    _termios.setRaw(true);
    _termios.setRawLength(1);
    _termios.setRawTimeout(0);
    applyTermios();
}

void SerialSensor::close() throw(n_u::IOException)
{
    shutdownPrompting();
    DSMSensor::close();
}

void SerialSensor::applyTermios() throw(nidas::util::IOException)
{
    if (_serialDevice) {
        _serialDevice->termios() = _termios;
        _serialDevice->applyTermios();
    }
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
            getLooper()->addClient(prompter,prompter->getPromptPeriodMsec());
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
	ostr << "<td align=left>" << _termios.getBaudRate() <<
		_termios.getParityString().substr(0,1) <<
		_termios.getDataBits() << _termios.getStopBits();
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
	    else if (aname == "baud") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail() || !_termios.setBaudRate(val))
		    throw n_u::InvalidParameterException(
		    	string("SerialSensor:") + getName(),
			aname,aval);
	    }
	    else if (aname == "parity") {
		if (aval == "odd") _termios.setParity(n_u::Termios::ODD);
		else if (aval == "even") _termios.setParity(n_u::Termios::EVEN);
		else if (aval == "none") _termios.setParity(n_u::Termios::NONE);
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
		_termios.setDataBits(val);
	    }
	    else if (aname == "stopbits") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			string("SerialSensor:") + getName(),
		    	aname, aval);
		_termios.setStopBits(val);
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
    strcpy(_prompt,val.c_str());
}

void SerialSensor::Prompter::setPromptPeriodMsec(const int val)
{
    _promptPeriodMsec = val;
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

