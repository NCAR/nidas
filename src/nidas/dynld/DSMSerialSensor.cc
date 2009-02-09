/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/rtlinux/dsm_serial_fifo.h>
#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/Looper.h>

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

DSMSerialSensor::DSMSerialSensor():_prompter(this),_prompting(false)
{
}

DSMSerialSensor::~DSMSerialSensor()
{
}

SampleScanner* DSMSerialSensor::buildSampleScanner()
{
    SampleScanner* scanr = CharacterSensor::buildSampleScanner();
    int bits = getDataBits() + getStopBits() + 1;
    switch(getParity()) {
    case n_u::Termios::ODD:
    case n_u::Termios::EVEN:
        bits++;
        break;
    case n_u::Termios::NONE:
        break;
    }

    int usecs = (bits * USECS_PER_SEC + getBaudRate() / 2) / getBaudRate();
    DLOG(("%s: baud=%d,bits=%d,usecsPerChar=%d",getName().c_str(),
	  getBaudRate(),bits,usecs));
    scanr->setUsecsPerByte(usecs);
    return scanr;
}
 

void DSMSerialSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    if (!isRTLinux()) flags |= O_NOCTTY;
    CharacterSensor::open(flags);

    if (isRTLinux()) rtlDevInit(flags);
    else unixDevInit(flags);

    sendInitString();

    if (isPrompted()) startPrompting();
}

void DSMSerialSensor::rtlDevInit(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
#ifdef DEBUG
    cerr << "sizeof(struct termios)=" << sizeof(struct termios) << endl;
    cerr << "termios=" << hex << getTermios() << endl;
    cerr << "c_iflag=" << &(getTermios()->c_iflag) << ' ' << getTermios()->c_iflag << endl;
    cerr << "c_oflag=" << &(getTermios()->c_oflag) << ' ' << getTermios()->c_oflag << endl;
    cerr << "c_cflag=" << &(getTermios()->c_cflag) << ' ' << getTermios()->c_cflag << endl;
    cerr << "c_lflag=" << &(getTermios()->c_lflag) << ' ' << getTermios()->c_lflag << endl;
    cerr << "c_line=" << (void *)&(getTermios()->c_line) << endl;
    cerr << "c_cc=" << (void *)&(getTermios()->c_cc[0]) << endl;

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

    ioctl(DSMSER_OPEN,&flags,sizeof(flags));

    // cerr << "DSMSER_TCSETS, SIZEOF=" << SIZEOF_TERMIOS << endl;
    ioctl(DSMSER_TCSETS,(void*)getTermios(),SIZEOF_TERMIOS);

    int latencyUsecs = (int)(getLatency() * USECS_PER_SEC);
    ioctl(DSMSER_SET_LATENCY,&latencyUsecs,sizeof(latencyUsecs));

    setMessageParameters();

    if (isPrompted()) {
	struct dsm_serial_prompt prompt;

	string nprompt = n_u::replaceBackslashSequences(getPromptString());

	strncpy(prompt.str,nprompt.c_str(),sizeof(prompt.str));
	prompt.len = nprompt.length();
	if (prompt.len > (int)sizeof(prompt.str))
		prompt.len = sizeof(prompt.str);

	enum irigClockRates erate = irigClockRateToEnum(
		(int)rint(getPromptRate()));

	if (fmodf(getPromptRate(),1.0) != 0.0 ||
		(getPromptRate() > 0.0 && erate == IRIG_NUM_RATES)) {
	    ostringstream ost;
	    ost << getPromptRate();
	    throw n_u::InvalidParameterException
	    	(getName(),"invalid prompt rate",ost.str());
	}
	prompt.rate = erate;
	ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));
    }
}

void DSMSerialSensor::unixDevInit(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{

    setFlowControl(n_u::Termios::NOFLOWCONTROL);
    setLocal(true);
    setRaw(true);
    if (getMessageLength() > 0) setRawLength(getMessageLength() +
    	getMessageSeparator().length());
    else setRawLength(1);
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

    if (::isatty(getReadFd())) {
        setTermios(getReadFd(),getName());

        int accmode = flags & O_ACCMODE;

        int fres;

        if (accmode == O_RDONLY) fres = ::tcflush(getReadFd(),TCIFLUSH);
        else if (accmode == O_WRONLY) fres = ::tcflush(getWriteFd(),TCOFLUSH);
        else fres = ::tcflush(getReadFd(),TCIOFLUSH);
        if (fres < 0) throw n_u::IOException(getName(),"tcflush",errno);
    }

    if (isPrompted()) {
        _prompter.setPrompt(n_u::replaceBackslashSequences(getPromptString()));
	_promptPeriodMsec = (int) rint(1000.0 / getPromptRate());
	// cerr << "promptPeriodMsec=" << _promptPeriodMsec << endl;
    }
}

void DSMSerialSensor::setMessageParameters()
    throw(nidas::util::IOException)
{
    if (getIODevice() && getReadFd() >= 0) {
        if (isRTLinux()) {
            struct dsm_serial_record_info recinfo;
            string nsep = getMessageSeparator();

            strncpy(recinfo.sep,nsep.c_str(),sizeof(recinfo.sep));
            recinfo.sepLen = nsep.length();
            if (recinfo.sepLen > (int)sizeof(recinfo.sep))
                recinfo.sepLen = sizeof(recinfo.sep);

            recinfo.atEOM = getMessageSeparatorAtEOM() ? 1 : 0;
            recinfo.recordLen = getMessageLength();
            ioctl(DSMSER_SET_RECORD_SEP,&recinfo,sizeof(recinfo));
        }
        else {
            if (getMessageLength() > 0) setRawLength(getMessageLength() +
                getMessageSeparator().length());
            else setRawLength(1);
            setTermios(getReadFd(),getName());
        }
    }
}

void DSMSerialSensor::close() throw(n_u::IOException)
{
    stopPrompting();
    if (isRTLinux()) 
	ioctl(DSMSER_CLOSE,0,0);
    DSMSensor::close();
}


void DSMSerialSensor::startPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
	if (isRTLinux()) ioctl(DSMSER_START_PROMPTER,0,0);
	else Looper::getInstance()->addClient(&_prompter,_promptPeriodMsec);
	_prompting = true;
    }
}

void DSMSerialSensor::stopPrompting() throw(n_u::IOException)
{
    if (isPrompted()) {
	if (isRTLinux()) ioctl(DSMSER_STOP_PROMPTER,0,0);
	else Looper::getInstance()->removeClient(&_prompter);
	_prompting = false;
    }
}


void DSMSerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct dsm_serial_status stat;
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
	if (isRTLinux()) {
	    ioctl(DSMSER_GET_STATUS,&stat,sizeof(stat));
	    ostr <<
		",il=" << stat.input_chars_lost <<
		",ol=" << stat.output_chars_lost <<
		",so=" << stat.sample_overflows <<
		",pe=" << stat.pe_cnt <<
		",oe=" << stat.oe_cnt <<
		",fe=" << stat.fe_cnt <<
		",mf=" << stat.min_fifo_usage <<
		",Mf=" << stat.max_fifo_usage <<
		",uqa=" << stat.uart_queue_avail <<
		",oqa=" << stat.output_queue_avail <<
		",tqa=" << stat.char_xmit_queue_avail;
	}
	else if (getTimeoutMsecs() > 0)
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

