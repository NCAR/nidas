/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <dsm_serial_fifo.h>
#include <dsm_serial.h>
#include <DSMSerialSensor.h>

#include <atdUtil/Logger.h>

#include <math.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_FUNCTION(DSMSerialSensor)

DSMSerialSensor::DSMSerialSensor():prompting(false)
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
    case atdTermio::Termios::ODD:
    case atdTermio::Termios::EVEN:
        bits++;
        break;
    case atdTermio::Termios::NONE:
        break;
    }

    int usecs = (int)rint((bits * USECS_PER_SEC + getBaudRate() / 2) /
    	getBaudRate());
#ifdef DEBUG
    atdUtil::Logger::getInstance()->log(LOG_DEBUG,
	"%s: baud=%d,bits=%d,usecsPerChar=%d\n",getName().c_str(),
	getBaudRate(),bits,usecs);
#endif
    scanr->setUsecsPerByte(usecs);
    return scanr;
}
 

void DSMSerialSensor::open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    if (!isRTLinux()) flags |= O_NOCTTY;
    DSMSensor::open(flags);

    if (isRTLinux()) rtlDevInit(flags);
    else unixDevInit(flags);

    init();

}

void DSMSerialSensor::rtlDevInit(int flags)
	throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
#ifdef DEBUG
    cerr << "sizeof(struct termios)=" << sizeof(struct termios) << endl;
    cerr << "termios=" << hex << getTermiosPtr() << endl;
    cerr << "c_iflag=" << &(getTermiosPtr()->c_iflag) << ' ' << getTermiosPtr()->c_iflag << endl;
    cerr << "c_oflag=" << &(getTermiosPtr()->c_oflag) << ' ' << getTermiosPtr()->c_oflag << endl;
    cerr << "c_cflag=" << &(getTermiosPtr()->c_cflag) << ' ' << getTermiosPtr()->c_cflag << endl;
    cerr << "c_lflag=" << &(getTermiosPtr()->c_lflag) << ' ' << getTermiosPtr()->c_lflag << endl;
    cerr << "c_line=" << (void *)&(getTermiosPtr()->c_line) << endl;
    cerr << "c_cc=" << (void *)&(getTermiosPtr()->c_cc[0]) << endl;

    cerr << "c_iflag=" << iflag() << endl;
    cerr << "c_oflag=" << oflag() << endl;
    cerr << "c_cflag=" << cflag() << endl;
    cerr << "c_lflag=" << lflag() << endl;
    cerr << "cfgetispeed=" << dec << cfgetispeed(getTermiosPtr()) << endl;
    cerr << "baud rate=" << getBaudRate() << endl;
    cerr << "data bits=" << getDataBits() << endl;
    cerr << "stop bits=" << getStopBits() << endl;
    cerr << "parity=" << getParityString() << endl;
#endif

    ioctl(DSMSER_OPEN,&flags,sizeof(flags));

    // cerr << "DSMSER_TCSETS, SIZEOF=" << SIZEOF_TERMIOS << endl;
    ioctl(DSMSER_TCSETS,(void*)getTermios(),SIZEOF_TERMIOS);

    long latencyUsecs = (long)(getLatency() * USECS_PER_SEC);
    ioctl(DSMSER_SET_LATENCY,&latencyUsecs,sizeof(latencyUsecs));

    /* send message separator information */
    struct dsm_serial_record_info recinfo;
    string nsep = getMessageSeparator();

    strncpy(recinfo.sep,nsep.c_str(),sizeof(recinfo.sep));
    recinfo.sepLen = nsep.length();
    if (recinfo.sepLen > (int)sizeof(recinfo.sep))
    	recinfo.sepLen = sizeof(recinfo.sep);

    recinfo.atEOM = getMessageSeparatorAtEOM() ? 1 : 0;
    recinfo.recordLen = getMessageLength();
    ioctl(DSMSER_SET_RECORD_SEP,&recinfo,sizeof(recinfo));

    if (isPrompted()) {
	struct dsm_serial_prompt prompt;

	string nprompt = DSMSensor::replaceBackslashSequences(getPromptString());

	strncpy(prompt.str,nprompt.c_str(),sizeof(prompt.str));
	prompt.len = nprompt.length();
	if (prompt.len > (int)sizeof(prompt.str))
		prompt.len = sizeof(prompt.str);

	enum irigClockRates erate = irigClockRateToEnum(
		(int)rint(getPromptRate()));

	if (fmod(getPromptRate(),1.0) != 0.0 ||
		(getPromptRate() > 0.0 && erate == IRIG_NUM_RATES)) {
	    ostringstream ost;
	    ost << getPromptRate();
	    throw atdUtil::InvalidParameterException
	    	(getName(),"invalid prompt rate",ost.str());
	}
	prompt.rate = erate;
	ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));

	startPrompting();
    }
}

void DSMSerialSensor::unixDevInit(int flags)
	throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{

    setFlowControl(atdTermio::Termios::NOFLOWCONTROL);
    setLocal(true);
    setRaw(true);
    if (getMessageLength() > 0) setRawLength(getMessageLength() +
    	getMessageSeparator().length());
    else setRawLength(4);

#ifdef DEBUG
    cerr << "c_iflag=" << iflag() << endl;
    cerr << "c_oflag=" << oflag() << endl;
    cerr << "c_cflag=" << cflag() << endl;
    cerr << "c_lflag=" << lflag() << endl;
    cerr << "cfgetispeed=" << dec << cfgetispeed(getTermiosPtr()) << endl;
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
    if (fres < 0) throw atdUtil::IOException(getName(),"tcflush",errno);

    // need to support prompting.
}

void DSMSerialSensor::close() throw(atdUtil::IOException)
{
    stopPrompting();
    if (isRTLinux()) 
	ioctl(DSMSER_CLOSE,0,0);
    DSMSensor::close();
}


void DSMSerialSensor::startPrompting() throw(atdUtil::IOException)
{
    if (isPrompted()) {
	ioctl(DSMSER_START_PROMPTER,0,0);
	prompting = true;
    }
}

void DSMSerialSensor::stopPrompting() throw(atdUtil::IOException)
{
    if (isPrompted()) {
	ioctl(DSMSER_STOP_PROMPTER,0,0);
	prompting = false;
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
	ostr << "</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
    }
}

void DSMSerialSensor::fromDOMElement(
	const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    CharacterSensor::fromDOMElement(node);

    XDOMElement xnode(node);

    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();

	    if (!aname.compare("ID"));
	    else if (!aname.compare("IDREF"));
	    else if (!aname.compare("class"));
	    else if (!aname.compare("devicename"));
	    else if (!aname.compare("id"));
	    else if (!aname.compare("baud")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail() || !setBaudRate(val))
		    throw atdUtil::InvalidParameterException(
		    	string("DSMSerialSensor:") + getName(),
			aname,aval);
	    }
	    else if (!aname.compare("parity")) {
		if (!aval.compare("odd")) setParity(ODD);
		else if (!aval.compare("even")) setParity(EVEN);
		else if (!aval.compare("none")) setParity(NONE);
		else throw atdUtil::InvalidParameterException(
		    string("DSMSerialSensor:") + getName(),
		    aname,aval);
	    }
	    else if (!aname.compare("databits")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(
			string("DSMSerialSensor:") + getName(),
		    	aname, aval);
		setDataBits(val);
	    }
	    else if (!aname.compare("stopbits")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(
			string("DSMSerialSensor:") + getName(),
		    	aname, aval);
		setStopBits(val);
	    }
	    else if (!aname.compare("nullterm"));
	    else if (!aname.compare("init_string"));
	    else if (!aname.compare("suffix"));
	    else throw atdUtil::InvalidParameterException(
		string("DSMSerialSensor:") + getName(),
		"unknown attribute",aname);

	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("message"));
	else if (!elname.compare("prompt"));
	else if (!elname.compare("sample"));
	else throw atdUtil::InvalidParameterException(
	    string("DSMSerialSensor:") + getName(),
	    "unknown element",elname);
    }
}

