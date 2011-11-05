/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/Looper.h>
#include <nidas/core/Prompt.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(DSMSerialSensor)

DSMSerialSensor::DSMSerialSensor():_prompting(false)
{
    setDefaultMode(O_RDWR);
}

DSMSerialSensor::~DSMSerialSensor()
{
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;
}

SampleScanner* DSMSerialSensor::buildSampleScanner()
	throw(n_u::InvalidParameterException)
{
    SampleScanner* scanr = CharacterSensor::buildSampleScanner();
    scanr->setUsecsPerByte(getUsecsPerByte());
    return scanr;
}

int DSMSerialSensor::getUsecsPerByte() const
{
    int usecs = 0;
    if (::isatty(getReadFd())) {
        int bits = getDataBits() + getStopBits() + 1;
        switch(getParity()) {
        case n_u::Termios::ODD:
        case n_u::Termios::EVEN:
            bits++;
            break;
        case n_u::Termios::NONE:
            break;
        }
        usecs = (bits * USECS_PER_SEC + getBaudRate() / 2) / getBaudRate();
    }
    return usecs;
}

void DSMSerialSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    flags |= O_NOCTTY;
    CharacterSensor::open(flags);

    unixDevInit(flags);

    sendInitString();

    initPrompting();
}

void DSMSerialSensor::unixDevInit(int flags)
	throw(n_u::IOException)
{
    if (::isatty(getReadFd())) {
        setFlowControl(n_u::Termios::NOFLOWCONTROL);
        setLocal(true);
        setRaw(true);
        setRawLength(1);
        setRawTimeout(0);

#ifdef DEBUG
        cerr << "c_iflag=" << iflag() << endl;
        cerr << "c_oflag=" << oflag() << endl;
        cerr << "c_cflag=" << cflag() << endl;
        cerr << "c_lflag=" << lflag() << endl;
        cerr << "cfgetispeed=" << dec << cfgetispeed(getTermios()) << endl;
        cerr << "baud rate=" << getBaudRate() << endl;
        cerr << "data bits=" << getDataBits() << endl;
        cerr << "stop bits=" << getStopBits() << endl;
        cerr << "parity=" << getParityString() << endl;
#endif

        // no latency support

        setTermios(getReadFd(),getName());

        int accmode = flags & O_ACCMODE;
        int fres;
        if (accmode == O_RDONLY) fres = ::tcflush(getReadFd(),TCIFLUSH);
        else if (accmode == O_WRONLY) fres = ::tcflush(getWriteFd(),TCOFLUSH);
        else fres = ::tcflush(getReadFd(),TCIOFLUSH);
        if (fres < 0) throw n_u::IOException(getName(),"tcflush",errno);
    }
}

void DSMSerialSensor::setMessageParameters(unsigned int len, const string& sep, bool eom)
    throw(n_u::InvalidParameterException, n_u::IOException)
{
    CharacterSensor::setMessageParameters(len,sep,eom);
    applyMessageParameters();
}

void DSMSerialSensor::applyMessageParameters()
    throw(nidas::util::IOException)
{
    if (getIODevice() && getReadFd() >= 0) {
        if (::isatty(getReadFd())) {
            setRaw(true);
            setRawLength(1);
            setRawTimeout(0);
            setTermios(getReadFd(),getName());
        }
    }
}

void DSMSerialSensor::close() throw(n_u::IOException)
{
    shutdownPrompting();
    DSMSensor::close();
}

void DSMSerialSensor::initPrompting() throw(n_u::IOException)
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

void DSMSerialSensor::shutdownPrompting() throw(n_u::IOException)
{
    stopPrompting();
    list<Prompter*>::const_iterator pi = _prompters.begin();
    for (; pi != _prompters.end(); ++pi) delete *pi;
    _prompters.clear();
}

void DSMSerialSensor::startPrompting() throw(n_u::IOException)
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

void DSMSerialSensor::stopPrompting() throw(n_u::IOException)
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

void DSMSerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    try {
	ostr << "<td align=left>" << getBaudRate() <<
		getParityString().substr(0,1) <<
		getDataBits() << getStopBits();
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

void DSMSerialSensor::fromDOMElement(
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
		if (ist.fail() || !setBaudRate(val))
		    throw n_u::InvalidParameterException(
		    	string("DSMSerialSensor:") + getName(),
			aname,aval);
	    }
	    else if (aname == "parity") {
		if (aval == "odd") setParity(ODD);
		else if (aval == "even") setParity(EVEN);
		else if (aval == "none") setParity(NONE);
		else throw n_u::InvalidParameterException(
		    string("DSMSerialSensor:") + getName(),
		    aname,aval);
	    }
	    else if (aname == "databits") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			string("DSMSerialSensor:") + getName(),
		    	aname, aval);
		setDataBits(val);
	    }
	    else if (aname == "stopbits") {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			string("DSMSerialSensor:") + getName(),
		    	aname, aval);
		setStopBits(val);
	    }
	    else if (aname == "nullterm");
	    else if (aname == "init_string");
	    else if (aname == "suffix");
	    else if (aname == "height");
	    else if (aname == "depth");
	    else if (aname == "duplicateIdOK");
	    else if (aname == "timeout");
	    else if (aname == "readonly");
	    else throw n_u::InvalidParameterException(
		string("DSMSerialSensor:") + getName(),
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
	    string("DSMSerialSensor:") + getName(),
	    "unknown element",elname);
    }
}

DSMSerialSensor::Prompter::~Prompter()
{
    delete [] _prompt;
}

void DSMSerialSensor::Prompter::setPrompt(const string& val)
{
    delete [] _prompt;
    _promptLen = val.length();
    _prompt = new char[_promptLen+1];
    strcpy(_prompt,val.c_str());
}

void DSMSerialSensor::Prompter::setPromptPeriodMsec(const int val)
{
    _promptPeriodMsec = val;
}

void DSMSerialSensor::Prompter::looperNotify() throw()
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

