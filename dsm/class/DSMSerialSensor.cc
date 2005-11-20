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
#include <RTL_DevIoctlStore.h>

#include <atdUtil/ThreadSupport.h>

#include <asm/ioctls.h>

#include <math.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_FUNCTION(DSMSerialSensor)

DSMSerialSensor::DSMSerialSensor()
{
}

DSMSerialSensor::~DSMSerialSensor() {
}

void DSMSerialSensor::open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    // It's magic, we can do an ioctl before the device is open!
    ioctl(DSMSER_OPEN,&flags,sizeof(flags));

    RTL_DSMSensor::open(flags);

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

    long latencyUsecs = (long)(MessageStreamSensor::getLatency() * USECS_PER_SEC);
    ioctl(DSMSER_SET_LATENCY,&latencyUsecs,sizeof(latencyUsecs));

    ioctl(DSMSER_TCSETS,getTermiosPtr(),SIZEOF_TERMIOS);

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

    if (MessageStreamSensor::isPrompted()) {
	struct dsm_serial_prompt prompt;

	string nprompt = MessageStreamSensor::replaceBackslashSequences(getPromptString());

	strncpy(prompt.str,nprompt.c_str(),sizeof(prompt.str));
	prompt.len = nprompt.length();
	if (prompt.len > (int)sizeof(prompt.str))
		prompt.len = sizeof(prompt.str);
	prompt.rate = getPromptRate();
	// cerr << "rate=" << prompt.rate << " prompt=\"" << nprompt << "\"" << endl;
	ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));

	startPrompting();
    }
    if (getInitString().length() > 0) {
	string initstr = replaceBackslashSequences(getInitString());
	write(initstr.c_str(),initstr.length());
    }
    init();
}

void DSMSerialSensor::close() throw(atdUtil::IOException)
{
    // cerr << "doing DSMSER_CLOSE" << endl;
    stopPrompting();
    ioctl(DSMSER_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

void DSMSerialSensor::startPrompting() throw(atdUtil::IOException)
{
    if (MessageStreamSensor::isPrompted()) {
        ioctl(DSMSER_START_PROMPTER,(const void*)0,0);
        prompting = true;
    }
}

void DSMSerialSensor::stopPrompting() throw(atdUtil::IOException)
{
    if (MessageStreamSensor::isPrompted()) {
        ioctl(DSMSER_STOP_PROMPTER,(const void*)0,0);
        prompting = false;
    }
}

void DSMSerialSensor::addSampleTag(SampleTag* tag)
	throw(atdUtil::InvalidParameterException)
{
    const string& sfmt = tag->getScanfFormat();
    if (sfmt.length() > 0) {
	if (MessageStreamSensor::getScanners().size() !=
		getSampleTags().size()) {
            ostringstream ost;
            ost << tag->getSampleId();
            throw atdUtil::InvalidParameterException(
		string("DSMSerialSensor:") + getName(),
		string("scanfFormat for sample id=") + ost.str(),
		"Either all samples for a MessageStreamSensor \
must have a scanfFormat or no samples");
        }

        MessageStreamSensor::addSampleTag(tag);
    }
    DSMSensor::addSampleTag(tag);
}

void DSMSerialSensor::init() throw(atdUtil::InvalidParameterException)
{
    MessageStreamSensor::init();
}


void DSMSerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct dsm_serial_status stat;
    try {
	ioctl(DSMSER_GET_STATUS,&stat,sizeof(stat));

	ostr << "<td align=left>" << getBaudRate() <<
		getParityString().substr(0,1) <<
		getDataBits() << getStopBits() << 
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
	    ",tqa=" << stat.char_xmit_queue_avail <<
	    "</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

void DSMSerialSensor::fromDOMElement(
	const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    RTL_DSMSensor::fromDOMElement(node);

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

    MessageStreamSensor::setSensorName(getName());

    MessageStreamSensor::fromDOMElement(node);

    // If sensor is prompted, set sampling rates for variables if unknown
    vector<SampleTag*>::const_iterator si;
    if (MessageStreamSensor::getPromptRate() != IRIG_ZERO_HZ) {
	float frate = irigClockEnumToRate(MessageStreamSensor::getPromptRate());
	for (si = sampleTags.begin(); si != sampleTags.end(); ++si) {
	    SampleTag* samp = *si;
	    if (samp->getRate() == 0.0) samp->setRate(frate);
	}
    }

    // make sure sampling rates are positive and equal
    float frate = -1.0;
    for (si = sampleTags.begin(); si != sampleTags.end(); ++si) {
	SampleTag* samp = *si;
	if (samp->getRate() <= 0.0)
	    throw atdUtil::InvalidParameterException(
		getName() + " has sample rate of 0.0");
	if (si == sampleTags.begin()) frate = samp->getRate();
	if (fabs((frate - samp->getRate()) / frate) > 1.e-3) {
	    ostringstream ost;
	    ost << frate << " & " << samp->getRate();
	    throw atdUtil::InvalidParameterException(
		getName() + " has different sample rates: " +
		    ost.str() + " samples/sec");
	}
    }
    if (sampleTags.size() == 1) sampleId = sampleTags.front()->getId();
    else sampleId = 0;
    // cerr << getName() << " sampleId=" << hex << sampleId << dec << endl;
}

DOMElement* DSMSerialSensor::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* DSMSerialSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

bool DSMSerialSensor::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    return scanMessageSample(samp,results);
}

