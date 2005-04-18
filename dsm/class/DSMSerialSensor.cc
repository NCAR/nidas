/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-01-03 13:26:59 -0700 (Mon, 03 Jan 2005) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSerialSensor.cc $

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

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_ENTRY_POINT(DSMSerialSensor)

DSMSerialSensor::DSMSerialSensor():
    sepAtEOM(true),
    messageLength(0),
    promptRate(IRIG_NUM_RATES),
    scanner(0),parsebuf(0),parsebuflen(0)
{
}

DSMSerialSensor::~DSMSerialSensor() {
    delete scanner;
    delete [] parsebuf;
}
void DSMSerialSensor::open(int flags) throw(atdUtil::IOException)
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

    ioctl(DSMSER_TCSETS,getTermiosPtr(),SIZEOF_TERMIOS);

    /* send message separator information */
    struct dsm_serial_record_info recinfo;
    string nsep = replaceEscapeSequences(getMessageSeparator());

    strncpy(recinfo.sep,nsep.c_str(),sizeof(recinfo.sep));
    recinfo.sepLen = nsep.length();
    if (recinfo.sepLen > (int)sizeof(recinfo.sep))
    	recinfo.sepLen = sizeof(recinfo.sep);

    recinfo.atEOM = getMessageSeparatorAtEOM() ? 1 : 0;
    recinfo.recordLen = getMessageLength();
    ioctl(DSMSER_SET_RECORD_SEP,&recinfo,sizeof(recinfo));

    /* a prompt rate of IRIG_NUM_RATES means no prompting */
    if (getPromptRate() != IRIG_NUM_RATES) {
	struct dsm_serial_prompt prompt;

	string nprompt = replaceEscapeSequences(getPromptString());

	strncpy(prompt.str,nprompt.c_str(),sizeof(prompt.str));
	prompt.len = nprompt.length();
	if (prompt.len > (int)sizeof(prompt.str))
		prompt.len = sizeof(prompt.str);
	prompt.rate = getPromptRate();
	// cerr << "rate=" << prompt.rate << " prompt=\"" << nprompt << "\"" << endl;
	ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));

	ioctl(DSMSER_START_PROMPTER,(const void*)0,0);
    }
}

void DSMSerialSensor::close() throw(atdUtil::IOException)
{
    // cerr << "doing DSMSER_CLOSE" << endl;
    ioctl(DSMSER_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

void DSMSerialSensor::setScanfFormat(const string& str)
    throw(atdUtil::InvalidParameterException)
{
    atdUtil::Synchronized autosync(scannerLock);
    if (!scanner) scanner = new AsciiScanner();
    
    try {
       scanner->setFormat(str);
    }
    catch (atdUtil::ParseException& pe) {
        throw atdUtil::InvalidParameterException(getName(),
               "setScanfFormat",pe.what());
    }
}

const string& DSMSerialSensor::getScanfFormat()
{
    static string emptyStr;
    atdUtil::Synchronized autosync(scannerLock);
    if (!scanner) return emptyStr;
    return scanner->getFormat();
}

void DSMSerialSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct dsm_serial_status stat;
    try {
	ioctl(DSMSER_GET_STATUS,&stat,sizeof(stat));

	ostr << "<td>" << getBaudRate() <<
		getParityString().substr(0,1) <<
		getDataBits() << getStopBits() << 
	    ",pe=" << stat.pe_cnt <<
	    ",oe=" << stat.oe_cnt <<
	    ",fe=" << stat.fe_cnt <<
	    ",iof=" << stat.input_char_overflows <<
	    ",oof=" << stat.output_char_overflows <<
	    ",so=" << stat.sample_overflows <<
	    ",ns=" << stat.nsamples << 
	    ",tql=" << stat.char_transmit_queue_length << 
	    ",tqs=" << stat.char_transmit_queue_size << 
	    ",sql=" << stat.sample_queue_length << 
	    ",sqs=" << stat.sample_queue_size << "</td>" << endl;
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
		    throw atdUtil::InvalidParameterException(getName(),"baud",
		    	aval);
	    }
	    else if (!aname.compare("parity")) {
		if (!aval.compare("odd")) setParity(ODD);
		else if (!aval.compare("even")) setParity(EVEN);
		else if (!aval.compare("none")) setParity(NONE);
		else throw atdUtil::InvalidParameterException
			(getName(),"parity",aval);
	    }
	    else if (!aname.compare("databits")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),
		    	"databits", aval);
		setDataBits(val);
	    }
	    else if (!aname.compare("stopbits")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),
		    	"stopbits", aval);
		setStopBits(val);
	    }
	    else if (!aname.compare("scanfFormat"))
		setScanfFormat(aval);
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
    list<SampleTag*>::const_iterator si;
    if (getPromptRate() != IRIG_NUM_RATES) {
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

string DSMSerialSensor::replaceEscapeSequences(string str)
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

bool DSMSerialSensor::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    // If no scanner defined, then don't scan sample, just pass it on
    if (!scanner) {
	DSMSensor::process(samp,results);
        return true;
    }

    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();

    if (slen > parsebuflen) {
	delete [] parsebuf;
	parsebuf = new char[slen + 1];
	parsebuflen = slen;
    }

    /*
     * t'would be nice to avoid this copy, but we must
     * null terminate before the scanf, and we haven't come
     * up with a general way to do it. Some serial sensors
     * output binary data, and we can't just add a 
     * null at the end of every serial record.
     */
    memcpy(parsebuf,(const char*)samp->getConstVoidDataPtr(),slen);
    parsebuf[slen] = '\0';

    int nfields = scanner->getNumberOfFields();

    SampleT<float>* outs = getSample<float>(nfields);

    int nparsed;
    {
	// Locking scannerLock here is overkill.
	// It allows changing the scanf string while we're running
	// which is probably not necessary.
	atdUtil::Synchronized autosync(scannerLock);
	nparsed = scanner->sscanf(parsebuf,outs->getDataPtr(),nfields);
    }

    if (!nparsed) {
	scanfFailures++;
	outs->freeReference();		// remember!
	return false;		// no sample
    }
    else if (nparsed != nfields) scanfPartials++;

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getSampleId());
    outs->setDataLength(nparsed);
    results.push_back(outs);
    return true;
}

