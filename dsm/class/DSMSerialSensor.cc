/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

// #define XML_DEBUG

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

DSMSerialSensor::DSMSerialSensor():
    sepAtEOM(true),
    messageLength(0),
    promptRate(IRIG_ZERO_HZ),
    maxScanfFields(0),
    parsebuf(0),parsebuflen(0),prompted(false),prompting(false),
    latency(0.1),nullTerminated(true)
{
}

DSMSerialSensor::~DSMSerialSensor() {
    delete [] parsebuf;
    std::list<AsciiScanner*>::iterator si;
    for (si = scanners.begin(); si != scanners.end(); ++si) {
        AsciiScanner* scanner = *si;
	delete scanner;
    }
}

void DSMSerialSensor::setMessageSeparator(const std::string& val)
{
    msgsep = replaceBackslashSequences(val);
}

const std::string& DSMSerialSensor::getMessageSeparator() const
{
    return msgsep;
}

const std::string DSMSerialSensor::getBackslashedMessageSeparator() const
{
    return addBackslashSequences(msgsep);
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

    long latencyUsecs = (long)(getLatency() * USECS_PER_SEC);
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

    /* a prompt rate of IRIG_ZERO_HZ means no prompting */
    prompted = getPromptRate() != IRIG_ZERO_HZ && getPromptString().size();
    if (prompted) {
	struct dsm_serial_prompt prompt;

	string nprompt = replaceBackslashSequences(getPromptString());

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
    if (prompted) {
	ioctl(DSMSER_START_PROMPTER,(const void*)0,0);
	prompting = true;
    }
}

void DSMSerialSensor::stopPrompting() throw(atdUtil::IOException)
{
    if (prompted) {
	ioctl(DSMSER_STOP_PROMPTER,(const void*)0,0);
	prompting = false;
    }
}

void DSMSerialSensor::togglePrompting() throw(atdUtil::IOException)
{
    if (prompting) stopPrompting();
    else startPrompting();
}

void DSMSerialSensor::addSampleTag(SampleTag* tag)
	throw(atdUtil::InvalidParameterException)
{

    const string& sfmt = tag->getScanfFormat();
    if (sfmt.length() > 0) {
	if (scanners.size() != sampleTags.size()) {
	    ostringstream ost;
	    ost << tag->getSampleId();
	    throw atdUtil::InvalidParameterException(getName(),
	       string("scanfFormat for sample id=") + ost.str(),
	       "Either all samples for a DSMSerialSensor must have a scanfFormat or no samples");
	}
	AsciiScanner* scanner = new AsciiScanner();
	try {
	   scanner->setFormat(replaceBackslashSequences(sfmt));
	}
	catch (atdUtil::ParseException& pe) {
	    throw atdUtil::InvalidParameterException(getName(),
		   "setScanfFormat",pe.what());
	}
	scanner->setSampleId(tag->getId());
	scanners.push_back(scanner);
	maxScanfFields = std::max(maxScanfFields,scanner->getNumberOfFields());
    }
    else if (scanners.size() > 0) {
	ostringstream ost;
	ost << tag->getSampleId();
	throw atdUtil::InvalidParameterException(getName(),
	   string("scanfFormat for sample id=") + ost.str(),
	   "Either all samples for a DSMSerialSensor must have a scanfFormat or no samples");
    }
    DSMSensor::addSampleTag(tag);
}

void DSMSerialSensor::init() throw(atdUtil::InvalidParameterException) {
    if (scanners.size() > 0) nextScanner = scanners.begin();

    /* if termination character is not CR or NL then set
     * nullTermination to false.  This over-rides whatever
     * the user set in the xml - because we know betta'.
     */
    int sl;
    if (getMessageSeparatorAtEOM() &&
    	(sl = getMessageSeparator().length()) > 0) {
	switch (getMessageSeparator()[sl-1]) {
	case '\r':
	case '\n':
	    break;
	default:
	    nullTerminated = false;
	    break;
	}
    }
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

	    if (!aname.compare("baud")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail() || !setBaudRate(val))
		    throw atdUtil::InvalidParameterException(getName(),aname,
		    	aval);
	    }
	    else if (!aname.compare("parity")) {
		if (!aval.compare("odd")) setParity(ODD);
		else if (!aval.compare("even")) setParity(EVEN);
		else if (!aval.compare("none")) setParity(NONE);
		else throw atdUtil::InvalidParameterException
			(getName(),aname,aval);
	    }
	    else if (!aname.compare("databits")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),
		    	aname, aval);
		setDataBits(val);
	    }
	    else if (!aname.compare("stopbits")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),
		    	aname, aval);
		setStopBits(val);
	    }
	    else if (!aname.compare("nullterm")) {
		istringstream ist(aval);
		ist >> nullTerminated;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),aname,
		    	aval);
	    }
	    else if (!aname.compare("init_string"))
		setInitString(aval);
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
#ifdef XML_DEBUG
	cerr << "element name=" << elname << endl;
#endif

	if (!elname.compare("message")) {
#ifdef XML_DEBUG
	    cerr << "message separator=" <<
	    	xchild.getAttributeValue("separator") << endl;
#endif
	    setMessageSeparator(xchild.getAttributeValue("separator"));

	    const string& str = xchild.getAttributeValue("position");
#ifdef XML_DEBUG
	    cerr << "separator position=" << str << endl;
#endif
	    if (!str.compare("beg")) setMessageSeparatorAtEOM(false);
	    else if (!str.compare("end")) setMessageSeparatorAtEOM(true);
	    else throw atdUtil::InvalidParameterException
			(getName(),"messageSeparator position",str);

#ifdef XML_DEBUG
	    cerr << "message length=" <<
	    	xchild.getAttributeValue("length") << endl;
#endif
	    istringstream ist(xchild.getAttributeValue("length"));
	    int val;
	    ist >> val;
	    if (ist.fail())
		throw atdUtil::InvalidParameterException(getName(),
		    "message length", xchild.getAttributeValue("length"));
	    setMessageLength(val);
	}
	else if (!elname.compare("prompt")) {
#ifdef XML_DEBUG
	    cerr << "prompt string=" << xchild.getAttributeValue("string") << endl;
#endif
	    std::string prompt = xchild.getAttributeValue("string");

	    setPromptString(prompt);

	    istringstream ist(xchild.getAttributeValue("rate"));
	    int rate;
	    ist >> rate;
	    if (ist.fail())
		throw atdUtil::InvalidParameterException(getName(),
		    "prompt rate", xchild.getAttributeValue("rate"));

	    enum irigClockRates erate = irigClockRateToEnum(rate);
#ifdef XML_DEBUG
	    cerr << "prompt rate=" << rate << " erate=" << erate << endl;
#endif

	    if (rate != 0 && erate == IRIG_NUM_RATES)
		throw atdUtil::InvalidParameterException
			(getName(),"prompt rate",
			    xchild.getAttributeValue("rate"));
	    setPromptRate(erate);
	}
    }

    // If sensor is prompted, set sampling rates for variables if unknown
    vector<SampleTag*>::const_iterator si;
    if (getPromptRate() != IRIG_ZERO_HZ) {
	float frate = irigClockEnumToRate(getPromptRate());
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

string DSMSerialSensor::replaceBackslashSequences(string str)
{
    unsigned int bs;
    for (unsigned int ic = 0; (bs = str.find('\\',ic)) != string::npos;
    	ic = bs) {
	bs++;
	if (bs == str.length()) break;
        switch(str[bs]) {
	case 'n':
	    str.erase(bs,1);
	    str[bs-1] = '\n';
	    break;
	case 'r':
	    str.erase(bs,1);
	    str[bs-1] = '\r';
	    break;
	case 't':
	    str.erase(bs,1);
	    str[bs-1] = '\t';
	    break;
	case '\\':
	    str.erase(bs,1);
	    str[bs-1] = '\\';
	    break;
	case 'x':	//  \xhh	hex
	    if (bs + 2 >= str.length()) break;
	    {
		istringstream ist(str.substr(bs+1,2));
		int hx;
		ist >> hex >> hx;
		if (!ist.fail()) {
		    str.erase(bs,3);
		    str[bs-1] = (char)(hx & 0xff);
		}
	    }
	    break;
	case '0':	//  \000   octal
	case '1':
	case '2':
	case '3':
	    if (bs + 2 >= str.length()) break;
	    {
		istringstream ist(str.substr(bs,3));
		int oc;
		ist >> oct >> oc;
		if (!ist.fail()) {
		    str.erase(bs,3);
		    str[bs-1] = (char)(oc & 0xff);
		}
	    }
	    break;
	}
    }
    return str;
}

string DSMSerialSensor::addBackslashSequences(string str)
{
    string res;
    for (unsigned int ic = 0; ic < str.length(); ic++) {
	char c = str[ic];
        switch(c) {
	case '\n':
	    res.append("\\n");
	    break;
	case '\r':
	    res.append("\\r");
	    break;
	case '\t':
	    res.append("\\t");
	    break;
	case '\\':
	    res.append("\\\\");
	    break;
	default:
	    if (::isprint(c)) res.push_back(c);
	    else {
		ostringstream ost;
		ost << "\\x" << hex << setw(2) << setfill('0') << (unsigned int) c;
		res.append(ost.str());
	    }
	        
	    break;
	}
    }
    return res;
}

bool DSMSerialSensor::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    // If no scanner defined, then don't scan sample, just pass it on
    if (!scanners.size()) {
	DSMSensor::process(samp,results);
        return true;
    }

    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();
    const char* inputstr = (const char*)samp->getConstVoidDataPtr();

    // copy the string if not null terminated.
    if (!nullTerminated) {
	if (slen > parsebuflen) {
	    delete [] parsebuf;
	    parsebuf = new char[slen + 1];
	    parsebuflen = slen;
	}

	memcpy(parsebuf,inputstr,slen);
	parsebuf[slen] = '\0';
	inputstr = parsebuf;
    }

    SampleT<float>* outs = getSample<float>(maxScanfFields);

    int nparsed = 0;
    for (unsigned int ntry = 0; ntry < scanners.size(); ntry++) {
	AsciiScanner* scanner = *nextScanner;
	nparsed = scanner->sscanf(inputstr,outs->getDataPtr(),
		scanner->getNumberOfFields());
	if (++nextScanner == scanners.end()) nextScanner = scanners.begin();
	if (nparsed > 0) {
	    outs->setId(scanner->getSampleId());
	    if (nparsed != scanner->getNumberOfFields()) scanfPartials++;
	    break;
	}
    }

    if (!nparsed) {
	scanfFailures++;
	outs->freeReference();		// remember!
	return false;		// no sample
    }

    outs->setTimeTag(samp->getTimeTag());
    outs->setDataLength(nparsed);
    results.push_back(outs);
    return true;
}

