/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

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
#include <XMLStringConverter.h>
#include <XDOM.h>

#include <atdUtil/ThreadSupport.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <asm/ioctls.h>

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
    scanner(0)
{
}

DSMSerialSensor::DSMSerialSensor(const string& nameArg) :
    RTL_DSMSensor(nameArg),
    sepAtEOM(true),
    messageLength(0),
    promptRate(IRIG_NUM_RATES),
    scanner(0)
{
}

DSMSerialSensor::~DSMSerialSensor() {
    delete scanner;
    try {
	close();
    }
    catch(atdUtil::IOException& ioe) {
      cerr << ioe.what() << endl;
    }
}
void DSMSerialSensor::open(int flags) throw(atdUtil::IOException)
{
  
    devIoctl = RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,portNum);
    if (devIoctl) devIoctl->open();

    ioctl(DSMSER_OPEN,&flags,sizeof(flags));
    cerr << "DSMSER_OPEN done" << endl;

    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }
    cerr << inFifoName << " opened" << endl;

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }
    cerr << outFifoName << " opened" << endl;

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
    cerr << dec;
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
	ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));

	ioctl(DSMSER_START_PROMPTER,(const void*)0,0);
    }
}

void DSMSerialSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing DSMSER_CLOSE" << endl;
    ioctl(DSMSER_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

void DSMSerialSensor::setScanfFormat(const string& str)
    throw(atdUtil::InvalidParameterException)
{
    scannerLock.lock();
    bool newscanner = (scanner == 0);
    scannerLock.unlock();

    if (newscanner) {
        SampleScanf* news = new SampleScanf();

	clistLock.lock();
	std::list<SampleClient*> tmp = clients;
	clistLock.unlock();

	std::list<SampleClient*>::iterator li;
	for (li = tmp.begin(); li != tmp.end(); ++li) {
	    removeSampleClient(*li);
	    news->addSampleClient(*li);
	}
	addSampleClient(news);

	scannerLock.lock();
	scanner = news;
	scannerLock.unlock();
    }
    try {
       scanner->setFormat(str);
    }
    catch (atdUtil::ParseException& pe) {
        throw atdUtil::InvalidParameterException("DSMSerialSensor",
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

/**
 * Override addSampleClient to re-direct the request
 * to my scanner.
 */
void DSMSerialSensor::addSampleClient(SampleClient* client) {
    atdUtil::Synchronized autosync(scannerLock);
    if (scanner) scanner->addSampleClient(client);
    else SampleSource::addSampleClient(client);
}

void DSMSerialSensor::removeSampleClient(SampleClient* client) {
    atdUtil::Synchronized autosync(scannerLock);
    if (scanner) scanner->removeSampleClient(client);
    else SampleSource::removeSampleClient(client);
}
  
void DSMSerialSensor::removeAllSampleClients() {
    atdUtil::Synchronized autosync(scannerLock);
    if (scanner) scanner->removeAllSampleClients();
    else SampleSource::removeAllSampleClients();
}

void DSMSerialSensor::fromDOMElement(
	const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    RTL_DSMSensor::fromDOMElement(node);
    XDOMElement xnode(node);

    cerr << "DSMSerialSensor::fromDOMElement element name=" <<
    	xnode.getNodeName() << endl;
	
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    cerr << "attrname=" << attr.getName() << endl;
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();
	    if (!aname.compare("baud")) {
		if (!setBaudRate(atoi(aval.c_str()))) 
		    throw atdUtil::InvalidParameterException
			("DSMSerialSensor","baud",aval);
	    }
	    else if (!aname.compare("parity")) {
		if (aval.compare("odd"))
			setParity(ODD);
		else if (aval.compare("even"))
			setParity(atdTermio::Termios::EVEN);
		else if (aval.compare("none"))
			setParity(atdTermio::Termios::NONE);
		else throw atdUtil::InvalidParameterException
			("DSMSerialSensor","parity",aval);
	    }
	    else if (!aname.compare("databits"))
		setDataBits(atoi(aval.c_str()));
	    else if (!aname.compare("stopbits"))
		setStopBits(atoi(aval.c_str()));
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
	cerr << "element name=" << elname << endl;

	if (!elname.compare("message")) {
	    cerr << "separator=" <<
	    	xchild.getAttributeValue("separator") << endl;
	    setMessageSeparator(xchild.getAttributeValue("separator"));

	    const string& str = xchild.getAttributeValue("position");
	    cerr << "position=" << str << endl;
	    if (!str.compare("beg")) setMessageSeparatorAtEOM(false);
	    else if (!str.compare("end")) setMessageSeparatorAtEOM(false);
	    else throw atdUtil::InvalidParameterException
			("DSMSerialSensor","messageSeparator position",str);

	    cerr << "length=" <<
	    	xchild.getAttributeValue("length") << endl;
	    setMessageLength(atoi(xchild.getAttributeValue("length").c_str()));
	}
	else if (!elname.compare("prompt")) {
	    cerr << "prompt=" << xchild.getAttributeValue("string") << endl;
	    std::string prompt = xchild.getAttributeValue("string");

	    setPromptString(prompt);

	    cerr << "rate=" << xchild.getAttributeValue("rate") << endl;
	    int rate = atoi(xchild.getAttributeValue("rate").c_str());
	    enum irigClockRates erate = irigClockRateToEnum(rate);

	    if (rate != 0 && erate == IRIG_NUM_RATES)
		throw atdUtil::InvalidParameterException
			("DSMSerialSensor","prompt rate",
			    xchild.getAttributeValue("rate"));
	    setPromptRate(erate);
	}
    }
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
