/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-02 10:36:18 -0700 (Fri, 02 Dec 2005) $

    $LastChangedRevision: 3165 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/CharacterSensor.cc $

 ******************************************************************
*/

#include <CharacterSensor.h>
#include <RTL_IODevice.h>
#include <UnixIODevice.h>

// #include <atdUtil/ThreadSupport.h>

// #include <asm/ioctls.h>

// #include <math.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace dsm;
using namespace xercesc;

CharacterSensor::CharacterSensor():
    rtlinux(-1),
    promptRate(0.0),
    maxScanfFields(0),
    prompted(false)
{
}

CharacterSensor::~CharacterSensor() {
    std::list<AsciiSscanf*>::iterator si;
    for (si = sscanfers.begin(); si != sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
	delete sscanf;
    }
}


bool CharacterSensor::isRTLinux() const
{
    if (rtlinux < 0)  {
	const string& dname = getDeviceName();
	unsigned int fs = dname.rfind('/');
	if (fs != string::npos && (fs + 6) < dname.length() &&
	    dname.substr(fs+1,6) == "dsmser")
		    rtlinux = 1;
	else rtlinux = 0;
    }
    return rtlinux == 1;
}

IODevice* CharacterSensor::buildIODevice() throw(atdUtil::IOException)
{
    if (isRTLinux()) return new RTL_IODevice();
    else return new UnixIODevice();
}

SampleScanner* CharacterSensor::buildSampleScanner()
{
    SampleScanner* scanr;

    if (isRTLinux()) scanr = new MessageSampleScanner();
    else scanr = new MessageStreamScanner();

    scanr->setMessageSeparator(getMessageSeparator());
    scanr->setMessageSeparatorAtEOM(getMessageSeparatorAtEOM());
    scanr->setMessageLength(getMessageLength());
    return scanr;
}
 
void CharacterSensor::addSampleTag(SampleTag* tag)
        throw(atdUtil::InvalidParameterException)
{

    DSMSensor::addSampleTag(tag);

    const string& sfmt = tag->getScanfFormat();
    if (sfmt.length() > 0) {
        AsciiSscanf* sscanf = new AsciiSscanf();
        try {
           sscanf->setFormat(replaceBackslashSequences(sfmt));
        }
        catch (atdUtil::ParseException& pe) {
            throw atdUtil::InvalidParameterException(getName(),
                   "setScanfFormat",pe.what());
        }
        sscanf->setSampleTag(tag);
        sscanfers.push_back(sscanf);
        maxScanfFields = std::max(maxScanfFields,sscanf->getNumberOfFields());
    }
    else if (sscanfers.size() > 0) {
        ostringstream ost;
        ost << tag->getSampleId();
        throw atdUtil::InvalidParameterException(getName(),
           string("scanfFormat for sample id=") + ost.str(),
           "Either all samples for a CharacterSensor \
must have a scanfFormat or no samples");
    }
}


void CharacterSensor::init() throw(atdUtil::InvalidParameterException)
{
    if (sscanfers.size() > 0) nextSscanfer = sscanfers.begin();

}

void CharacterSensor::fromDOMElement(
	const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    DSMSensor::fromDOMElement(node);

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

	    if (!aname.compare("nullterm")) { }
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
			(getName(),"messageSeparator position",str);

	    istringstream ist(xchild.getAttributeValue("length"));
	    int val;
	    ist >> val;
	    if (ist.fail())
		throw atdUtil::InvalidParameterException(getName(),
		    "message length", xchild.getAttributeValue("length"));
	    setMessageLength(val);
	}
	else if (!elname.compare("prompt")) {
	    std::string prompt = xchild.getAttributeValue("string");

	    setPromptString(prompt);

	    istringstream ist(xchild.getAttributeValue("rate"));
	    float rate;
	    ist >> rate;
	    if (ist.fail())
		throw atdUtil::InvalidParameterException(getName(),
		    "prompt rate", xchild.getAttributeValue("rate"));

	    if (rate < 0.0)
		throw atdUtil::InvalidParameterException
			(getName(),"prompt rate",
			    xchild.getAttributeValue("rate"));
	    setPromptRate(rate);
	}
    }
    /* a prompt rate of 0 means no prompting */
    prompted = getPromptRate() > 0.0 && getPromptString().size();
    // If sensor is prompted, set sampling rates for variables if unknown
    set<SampleTag*>::const_iterator si;
    if (getPromptRate() > 0.0) {
        for (si = getncSampleTags().begin();
		si != getncSampleTags().end(); ++si) {
            SampleTag* samp = *si;
            if (samp->getRate() == 0.0) samp->setRate(getPromptRate());
        }

	// make sure prompted sampling rates are positive and equal
	for (si = getncSampleTags().begin();
	    si != getncSampleTags().end(); ++si) {
	    SampleTag* samp = *si;
	    if (samp->getRate() <= 0.0)
		throw atdUtil::InvalidParameterException(
		    getName() + " prompted sensor has sample rate <= 0.0");
	    if (fabs((getPromptRate() - samp->getRate()) / getPromptRate()) > 1.e-3) {
		ostringstream ost;
		ost << " prompt rate=" << getPromptRate() <<
			", sample rate= " << samp->getRate();
		throw atdUtil::InvalidParameterException(
		    getName() + ost.str() + " samples/sec");
	    }
	}
    }
}

DOMElement* CharacterSensor::toDOMParent(
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

DOMElement* CharacterSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

bool CharacterSensor::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    assert(samp->getType() == CHAR_ST);
    const char* inputstr = (const char*)samp->getConstVoidDataPtr();

    SampleT<float>* outs = getSample<float>(maxScanfFields);

    const SampleTag* stag = 0;
    int nparsed = 0;
    for (unsigned int ntry = 0; ntry < sscanfers.size(); ntry++) {
	AsciiSscanf* sscanf = *nextSscanfer;
	nparsed = sscanf->sscanf(inputstr,outs->getDataPtr(),
		sscanf->getNumberOfFields());
	if (++nextSscanfer == sscanfers.end()) nextSscanfer = sscanfers.begin();
	if (nparsed > 0) {
	    stag = sscanf->getSampleTag();
	    outs->setId(stag->getId());
	    if (nparsed != sscanf->getNumberOfFields()) scanfPartials++;
	    break;
	}
    }

    if (!nparsed) {
	scanfFailures++;
	outs->freeReference();	// remember!
	return false;		// no sample
    }

    float* fp = outs->getDataPtr();
    const vector<const Variable*>& vars = stag->getVariables();
    for (int i = 0; i < nparsed && i < (signed)vars.size(); i++,fp++) {
	const VariableConverter* conv = vars[i]->getConverter();
	if (conv) *fp = conv->convert(*fp);
    }
    outs->setTimeTag(samp->getTimeTag());
    outs->setDataLength(nparsed);
    results.push_back(outs);
    return true;
}

