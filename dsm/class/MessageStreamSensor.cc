/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <MessageStreamSensor.h>

#include <atdUtil/ThreadSupport.h>

// #include <asm/ioctls.h>

// #include <math.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace dsm;
using namespace xercesc;

MessageStreamSensor::MessageStreamSensor():
    sepAtEOM(true),
    messageLength(0),
    promptRate(IRIG_ZERO_HZ),
    maxScanfFields(0),
    parsebuf(0),parsebuflen(0),prompted(false),
    latency(0.1),nullTerminated(true),
    BUFSIZE(8192),MAX_MESSAGE_STREAM_SAMPLE_SIZE(8192),
    buffer(0),separatorCnt(0),separator(0),osamp(0),sampleOverflows(0)

{
}

MessageStreamSensor::~MessageStreamSensor() {
    delete [] parsebuf;
    std::list<AsciiScanner*>::iterator si;
    for (si = scanners.begin(); si != scanners.end(); ++si) {
        AsciiScanner* scanner = *si;
	delete scanner;
    }
    delete [] buffer;
    delete [] separator;
    if (osamp) osamp->freeReference();
}

void MessageStreamSensor::setMessageSeparator(const std::string& val)
{
    separatorString = replaceBackslashSequences(val);

    separatorLen = separatorString.length();
    delete [] separator;
    separator = new char[separatorLen+1];
    strcpy(separator,separatorString.c_str());
}

const std::string& MessageStreamSensor::getMessageSeparator() const
{
    return separatorString;
}

const std::string MessageStreamSensor::getBackslashedMessageSeparator() const
{
    return addBackslashSequences(separatorString);
}

void MessageStreamSensor::addSampleTag(SampleTag* tag)
        throw(atdUtil::InvalidParameterException)
{

    const string& sfmt = tag->getScanfFormat();
    if (sfmt.length() > 0) {
        AsciiScanner* scanner = new AsciiScanner();
        try {
           scanner->setFormat(replaceBackslashSequences(sfmt));
        }
        catch (atdUtil::ParseException& pe) {
            throw atdUtil::InvalidParameterException(getSensorName(),
                   "setScanfFormat",pe.what());
        }
        scanner->setSampleId(tag->getId());
        scanners.push_back(scanner);
        maxScanfFields = std::max(maxScanfFields,scanner->getNumberOfFields());
    }
    else if (scanners.size() > 0) {
        ostringstream ost;
        ost << tag->getSampleId();
        throw atdUtil::InvalidParameterException(getSensorName(),
           string("scanfFormat for sample id=") + ost.str(),
           "Either all samples for a DSMSerialSensor \
must have a scanfFormat or no samples");
    }
}


void MessageStreamSensor::init() throw(atdUtil::InvalidParameterException) {
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
    else nullTerminated = false;

    delete [] buffer;
    buffer = new char[BUFSIZE];
}

void MessageStreamSensor::fromDOMElement(
	const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

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

	    if (!aname.compare("nullterm")) {
                istringstream ist(aval);
                ist >> nullTerminated;
                if (ist.fail())
                    throw atdUtil::InvalidParameterException(getSensorName(),aname,
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

	if (!elname.compare("message")) {
	    setMessageSeparator(xchild.getAttributeValue("separator"));

	    const string& str = xchild.getAttributeValue("position");
	    if (!str.compare("beg")) setMessageSeparatorAtEOM(false);
	    else if (!str.compare("end")) setMessageSeparatorAtEOM(true);
	    else throw atdUtil::InvalidParameterException
			(getSensorName(),"messageSeparator position",str);

	    istringstream ist(xchild.getAttributeValue("length"));
	    int val;
	    ist >> val;
	    if (ist.fail())
		throw atdUtil::InvalidParameterException(getSensorName(),
		    "message length", xchild.getAttributeValue("length"));
	    setMessageLength(val);
	}
	else if (!elname.compare("prompt")) {
	    std::string prompt = xchild.getAttributeValue("string");

	    setPromptString(prompt);

	    istringstream ist(xchild.getAttributeValue("rate"));
	    int rate;
	    ist >> rate;
	    if (ist.fail())
		throw atdUtil::InvalidParameterException(getSensorName(),
		    "prompt rate", xchild.getAttributeValue("rate"));

	    enum irigClockRates erate = irigClockRateToEnum(rate);

	    if (rate != 0 && erate == IRIG_NUM_RATES)
		throw atdUtil::InvalidParameterException
			(getSensorName(),"prompt rate",
			    xchild.getAttributeValue("rate"));
	    setPromptRate(erate);
	}
    }
    /* a prompt rate of IRIG_ZERO_HZ means no prompting */
    prompted = getPromptRate() != IRIG_ZERO_HZ && getPromptString().size();
}

DOMElement* MessageStreamSensor::toDOMParent(
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

DOMElement* MessageStreamSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

string MessageStreamSensor::replaceBackslashSequences(string str)
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

string MessageStreamSensor::addBackslashSequences(string str)
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

bool MessageStreamSensor::scanMessageSample(const Sample* samp,list<const Sample*>& results)
	throw()
{
    assert(samp->getType() == CHAR_ST);
    const char* inputstr = (const char*)samp->getConstVoidDataPtr();
    size_t slen = samp->getDataLength();

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


dsm_time_t MessageStreamSensor::readSamplesSepBOM(SampleDater* dater,
	DSMSensor* sensor) throw (atdUtil::IOException)
{
    dsm_time_t tfirstchar = getSystemTime();

    dsm_time_t ttres = 0;

    size_t rlen = sensor->read(buffer,BUFSIZE);
    const char* eob = buffer + rlen;
    tfirstchar -= rlen * usecsPerChar;

    sampleLengthAlloc = std::max(messageLength,separatorLen + 2);
    if (!osamp) {
	osamp = getSample<char>(sampleLengthAlloc);
	outSampLen = 0;
	outSampDataPtr = osamp->getDataPtr();
    }

    int maxadd = std::max((nullTerminated ? 2 : 1),separatorLen);

    for (register const char* cp = buffer; cp < eob; cp++) {
        register unsigned char c = *cp;

	// check if we need to enlarge sample
	if (outSampLen + maxadd >= osamp->getAllocByteLength()) {

	    if (osamp->getAllocByteLength() ==
	    	MAX_MESSAGE_STREAM_SAMPLE_SIZE) {

		// reached maximum sample size
		sampleOverflows++;

		if (nullTerminated) outSampDataPtr[outSampLen-1] =
		    '\0';
		osamp->setDataLength(outSampLen);
		// assert: if outSampLen is > 0 then bomtt is valid.
		osamp->setTimeTag(bomtt);
		ttres = osamp->getTimeTag();
		sensor->distribute(osamp);

		osamp = getSample<char>(sampleLengthAlloc);
		outSampLen = 0;
		outSampDataPtr = osamp->getDataPtr();
		separatorCnt = 0;	// start over
	    }
	    else {		// allocate more space in sample
		size_t newlen = outSampLen + outSampLen / 2;
		if (newlen > MAX_MESSAGE_STREAM_SAMPLE_SIZE)
			newlen = MAX_MESSAGE_STREAM_SAMPLE_SIZE;
		osamp->allocateData(newlen);
		outSampDataPtr = osamp->getDataPtr();
	    }
	}

       // loop until we've figured out what to do with this character
        for (;;) {
            if (separatorCnt < separatorLen) {
                // This block is entered if we are currently scanning
                // for the message separator at the beginning of the message.
                if (c == separator[separatorCnt]) {
                    // We now have a character match to the record separator.
                    // increment the separator counter.
                    // if matched entire separator string, previous sample
                    // is ready, ship it.

                    // the receipt time of the initial separator character
		    // is the timetag for the sample.
		    // We haven't yet sent out the previous sample
		    // because we're waiting for a complete match
		    // to the separator string, so we have to
		    // save this timetag.
                    if (separatorCnt == 0) bomtt =
			tfirstchar + (cp - buffer) * usecsPerChar;

                    if (++separatorCnt == separatorLen) {
			// send previous
                        if (outSampLen > 0) {
			    if (nullTerminated) outSampDataPtr[outSampLen++] =
			    	'\0';
			    osamp->setDataLength(outSampLen);
			    ttres = osamp->getTimeTag();
			    sensor->distribute(osamp);

			    // readjust sampleLengthAlloc if this
			    // good sample was bigger.
			    if (outSampLen > sampleLengthAlloc)
				sampleLengthAlloc = std::min(
				    outSampLen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
			    osamp = getSample<char>(sampleLengthAlloc);
			    outSampLen = 0;
			    outSampDataPtr = osamp->getDataPtr();

			}
                        osamp->setTimeTag(bomtt);
                        // copy separator to next sample
                        ::memcpy(outSampDataPtr+outSampLen,separator,
                                separatorCnt);
                        outSampLen += separatorCnt;
                        // leave separatorCnt equal to separatorLen
                    }
                    break;              // character was input
                }
	        else {
                    // At this point:
                    // 1. we're expecting the BOM separator, but
                    // 2. the current character fails a match with the
                    //    BOM string
                    // We'll send the faulty data along anyway so that
                    // the user can see what is going on.

                    if (separatorCnt > 0) {     // previous partial match
                        // We have a partial match to separator,
			// copy chars to the sample data.
                        ::memcpy(outSampDataPtr+outSampLen,separator,
                                separatorCnt);
                        outSampLen += separatorCnt;
                        separatorCnt = 0;	// start looking at beg
                        // keep trying matching first character of separator
                        // this won't infinitely loop because now
			// separatorCnt=0
                    }
                    else {              // no match to first character in sep
			if (outSampLen == 0) bomtt =
			    tfirstchar + (cp - buffer) * usecsPerChar;
			outSampDataPtr[outSampLen++] = c;
			break;                  // character was input
		    }
		}
	    }
	    else {
		// At this point we have a match to the BOM separator string.
		// and are filling the data buffer.

		// if a variable record length, then check to see
		// if the character matches the initial character
		// in the separator string.
		if (messageLength == 0) {
		    if (c == separator[0]) {    // first char of next
			separatorCnt = 0;
			// loop again to try match to first character
		    }
		    else {
			// no match, treat as data
			outSampDataPtr[outSampLen++] = c;
			break;
		    }
		}
		else {
		    // fixed record length, save character in buffer.
		    // no chance for overflow here, as long as
		    // messageLen < sizeof(buffer)
		    outSampDataPtr[outSampLen++] = c;
		    // If we're done, scan for separator next.
		    if (outSampLen == (unsigned) messageLength)
			separatorCnt = 0;
		    break;
		}
	    }
	}       // loop until we do something with character
    }           // loop over characters in buffer
    return ttres;
}
dsm_time_t MessageStreamSensor::readSamplesSepEOM(SampleDater* dater,
	DSMSensor* sensor) throw (atdUtil::IOException)
{
    dsm_time_t tfirstchar = getSystemTime();

    dsm_time_t ttres = 0;

    size_t rlen = sensor->read(buffer,BUFSIZE);
    const char* eob = buffer + rlen;
    tfirstchar -= rlen * usecsPerChar; 

    sampleLengthAlloc = std::max(messageLength,separatorLen + 2);
    if (!osamp) {
	osamp = getSample<char>(sampleLengthAlloc);
	outSampLen = 0;
	outSampDataPtr = osamp->getDataPtr();
    }

    // maximum number of characters added to sample buffer
    // in each loop iteration.
    int maxadd = nullTerminated ? 2 : 1;

    for (register const char* cp = buffer; cp < eob; cp++) {
        register unsigned char c = *cp;

	// check if we need to enlarge sample
	if (outSampLen + maxadd >= osamp->getAllocByteLength()) {
	    // reached maximum sample size
	    if (osamp->getAllocByteLength() ==
	    	MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
		sampleOverflows++;

		if (nullTerminated) outSampDataPtr[outSampLen-1] =
		    '\0';
		osamp->setDataLength(outSampLen);
		ttres = osamp->getTimeTag();
		sensor->distribute(osamp);

		osamp = getSample<char>(sampleLengthAlloc);
		outSampLen = 0;
		separatorCnt = 0;
		outSampDataPtr = osamp->getDataPtr();

	    }
	    else {		// allocate more space

		size_t newlen = outSampLen + outSampLen / 2;
		if (newlen > MAX_MESSAGE_STREAM_SAMPLE_SIZE)
			newlen = MAX_MESSAGE_STREAM_SAMPLE_SIZE;
		osamp->allocateData(newlen);
		outSampDataPtr = osamp->getDataPtr();
	    }
	}

	if (outSampLen == 0)
	    osamp->setTimeTag(tfirstchar +
		(cp - buffer) * usecsPerChar);
	outSampDataPtr[outSampLen++] = c;

	if (messageLength == 0) {
	    // if a variable record length, then check to see
	    // if the character matches the current character
	    // in the end of message separator string.
	    if (c == separator[separatorCnt]) {
		// character matched
		if (++separatorCnt == separatorLen) {
		    // sample is ready
		    if (nullTerminated)
			outSampDataPtr[outSampLen++] = '\0';
		    osamp->setDataLength(outSampLen);
		    ttres = osamp->getTimeTag();
		    sensor->distribute(osamp);

		    osamp = getSample<char>(sampleLengthAlloc);
		    outSampLen = 0;
		    outSampDataPtr = osamp->getDataPtr();
		    separatorCnt = 0;
		}
	    }
	    else {
		// variable record length, no match of current character
		// to EOM string.

		// check for match at beginning of separator string
		// since sepcnt > 0 we won't have a complete match since
		// sepLen then must be > 1.
		if (separatorCnt > 0 &&
		      c == separator[separatorCnt = 0]) separatorCnt++;
	    }
	}
	else {
	    // fixed length record
	    if (outSampLen >= (unsigned) messageLength) {
		// we've read in all our characters, now scan separator string
		// if no match, check from beginning of separator string
		if (c == separator[separatorCnt] ||
		    c == separator[separatorCnt = 0]) {
		    if (++separatorCnt == separatorLen) {
			if (nullTerminated)
			    outSampDataPtr[outSampLen++] = '\0';
			osamp->setDataLength(outSampLen);
			ttres = osamp->getTimeTag();
			sensor->distribute(osamp);

			osamp = getSample<char>(sampleLengthAlloc);
			outSampLen = 0;
			outSampDataPtr = osamp->getDataPtr();
			separatorCnt = 0;
		    }
		}
	    }
	}
    }
    return ttres;
}
