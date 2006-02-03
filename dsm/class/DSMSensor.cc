
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <DSMSensor.h>
#include <DSMConfig.h>

#include <dsm_sample.h>
#include <SamplePool.h>
#include <atdUtil/Logger.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <set>

using namespace std;
using namespace dsm;
using namespace xercesc;

/* static */
bool DSMSensor::zebra = false;

DSMSensor::DSMSensor() :
    iodev(0),scanner(0),dsm(0),id(0),
    latency(0.1)	// default sensor latency, 0.1 secs
{
}


DSMSensor::~DSMSensor()
{

    for (vector<SampleTag*>::const_iterator si = sampleTags.begin();
    	si != sampleTags.end(); ++si) delete *si;
    delete scanner;
    delete iodev;
}

void DSMSensor::addSampleTag(SampleTag* tag)
	throw(atdUtil::InvalidParameterException)
{
    sampleTags.push_back(tag);
    constSampleTags.push_back(tag);
}

/**
 * Fetch the DSM name.
 */
const std::string& DSMSensor::getDSMName() const {
    static std::string unk("unknown");
    if (dsm) return dsm->getName();
    return unk;
}

/*
 * If location is an empty string, return DSMConfig::getLocation()
 */
const std::string& DSMSensor::getLocation() const {
    if (location.length() == 0 && dsm) return dsm->getLocation();
    return location;
}

/*
 * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
 */
void DSMSensor::open(int flags)
	throw(atdUtil::IOException,atdUtil::InvalidParameterException) 
{
    if (!iodev) iodev = buildIODevice();
    cerr << "iodev->setName " << getDeviceName() << endl;
    iodev->setName(getDeviceName());

    atdUtil::Logger::getInstance()->log(LOG_NOTICE,
    	"opening: %s",getDeviceName().c_str());

    iodev->open(flags);
    if (!scanner) scanner = buildSampleScanner();
    scanner->init();
}

/*
 * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
 */
void DSMSensor::close() throw(atdUtil::IOException) 
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
    	"closing: %s",getDeviceName().c_str());
    iodev->close();
}

void DSMSensor::init() throw(atdUtil::InvalidParameterException)
{
}

bool DSMSensor::receive(const Sample *samp) throw()
{
    list<const Sample*> results;
    process(samp,results);
    distribute(results);	// distribute on a list does the freeReference
    return true;
}

/**
 * Default implementation of process just passes samples on.
 */
bool DSMSensor::process(const Sample* s, list<const Sample*>& result) throw()
{
    s->holdReference();
    result.push_back(s);
    return true;
}

void DSMSensor::printStatusHeader(std::ostream& ostr) throw()
{
  static char *glyph[] = {"\\","|","/","-"};
  static int anim=0;
  if (++anim == 4) anim=0;
  zebra = false;

  string dsm_name(getDSMConfig()->getName());
  string dsm_lctn(getDSMConfig()->getLocation());

    ostr <<
"<table id=\"sensor_status\">\
<caption>"+dsm_lctn+" ("+dsm_name+") "+glyph[anim]+"</caption>\
<tr>\
<th>name</th>\
<th>samp/sec</th>\
<th>byte/sec</th>\
<th>min&nbsp;samp<br>length</th>\
<th>max&nbsp;samp<br>length</th>\
<th>bad<br>timetags</th>\
<th>extended&nbsp;status</th>\
<tbody align=center>" << endl;	// default alignment in table body
}

void DSMSensor::printStatusTrailer(std::ostream& ostr) throw()
{
    ostr << "</tbody></table>" << endl;
}
void DSMSensor::printStatus(std::ostream& ostr) throw()
{
    string oe(zebra?"odd":"even");
    zebra = !zebra;
    ostr <<
        "<tr class=\"" << oe << "\"><td align=left>" <<
                getDeviceName() << ',' <<
		(getCatalogName().length() > 0 ?
			getCatalogName() : getClassName()) <<
		"</td>" << endl <<
    	"<td>" << fixed << setprecision(2) <<
		getObservedSamplingRate() << "</td>" << endl <<
    	"<td>" << setprecision(0) <<
		getObservedDataRate() << "</td>" << endl <<
	"<td>" << getMinSampleLength() << "</td>" << endl <<
	"<td>" << getMaxSampleLength() << "</td>" << endl <<
	"<td>" << getBadTimeTagCount() << "</td>" << endl;
}

void DSMSensor::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    /* If a catalog entry exists for a DSMSensor, then this
     * fromDOMElement will be called twice, first for the
     * catalog DOMElement, and then for the actual <sensor>
     * tag of the <dsm>.
     */
    setDSMId(getDSMConfig()->getId());

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    if (!attr.getName().compare("devicename"))
		setDeviceName(attr.getValue());
	    // set the catalog name from the ID
	    else if (!attr.getName().compare("ID"))
		setCatalogName(attr.getValue());
	    else if (!attr.getName().compare("class")) {
	        if (getClassName().length() == 0)
		    setClassName(attr.getValue());
		else if (getClassName().compare(attr.getValue()))
		    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    	"class attribute=%s does not match getClassName()=%s\n",
			attr.getValue().c_str(),getClassName().c_str());
	    }
	    else if (!attr.getName().compare("location"))
		setLocation(attr.getValue());
	    else if (!attr.getName().compare("id")) {
		istringstream ist(attr.getValue());
		// If you unset the dec flag, then a leading '0' means
		// octal, and 0x means hex.
		ist.unsetf(ios::dec);
		unsigned short val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("sensor",
		    	attr.getName(),attr.getValue());
		setShortId(val);
	    }
	    else if (!attr.getName().compare("latency")) {
		istringstream ist(attr.getValue());
		float val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("sensor",
		    	attr.getName(),attr.getValue());
		setLatency(val);
	    }
	    else if (!attr.getName().compare("suffix"))
		setSuffix(attr.getValue());
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("sample")) {
	    SampleTag* newtag = new SampleTag();
	    newtag->fromDOMElement((DOMElement*)child);
	    if (newtag->getSampleId() == 0) {
		delete newtag;
		throw atdUtil::InvalidParameterException(
		    getName(),"sample id invalid or not found","0");
	    }

	    for (vector<SampleTag*>::const_iterator si = sampleTags.begin();
		si != sampleTags.end(); ++si) {
		SampleTag* stag = *si;
		// If a sample id matches a previous one (most likely the
		// catalog) then update it from this DOMElement.
		if (stag->getSampleId() == newtag->getSampleId()) {
		    // update the sample with the new DOMElement
		    stag->fromDOMElement((DOMElement*)child);
		    stag->setDSMId(getDSMConfig()->getId());
		    stag->setSensorId(getShortId());
		    delete newtag;
		    newtag = 0;
		    break;
		}
	    }
	    if (newtag) {
		newtag->setDSMId(getDSMConfig()->getId());
		newtag->setSensorId(getShortId());
		addSampleTag(newtag);
	    }
	}
    }

    // sensors in the catalog may not have any sample tags
    // so at this point it is OK if sampleTags.size() == 0.

    // Check that sample ids are unique for this sensor.
    set<unsigned short> ids;
    for (vector<SampleTag*>::const_iterator si = sampleTags.begin();
    	si != sampleTags.end(); ++si) {
	SampleTag* stag = *si;

	if (getShortId() == 0) throw atdUtil::InvalidParameterException(
	    	getName(),"id","zero or missing");

	// set the suffix
	if (getSuffix().length() > 0) stag->setSuffix(getSuffix());

	pair<set<unsigned short>::const_iterator,bool> ins =
		ids.insert(stag->getId());
	if (!ins.second) {
	    ostringstream ost;
	    ost << stag->getId();
	    throw atdUtil::InvalidParameterException(
	    	getName(),"duplicate sample id", ost.str());
	}
    }
}

DOMElement* DSMSensor::toDOMParent(
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

DOMElement* DSMSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

/* static */
string DSMSensor::replaceBackslashSequences(string str)
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

/* static */
string DSMSensor::addBackslashSequences(string str)
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
	    // in C locale isprint returns true for
	    // alpha-numeric, punctuation and the space character
	    // but not other white-space characters like tabs,
	    // newlines, carriage-returns,  form-feeds, etc.
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
