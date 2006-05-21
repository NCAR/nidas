
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
#include <Site.h>
#include <NidsIterators.h>

#include <dsm_sample.h>
#include <SamplePool.h>
#include <atdUtil/Logger.h>

#include <math.h>
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
    rawSampleTag(0),
    height(floatNAN),
    latency(0.1)	// default sensor latency, 0.1 secs
{
}


DSMSensor::~DSMSensor()
{

    for (set<SampleTag*>::const_iterator si = sampleTags.begin();
    	si != sampleTags.end(); ++si) delete *si;
    delete rawSampleTag;
    delete scanner;
    delete iodev;

    map<std::string,Parameter*>::const_iterator pi;
    for (pi = parameters.begin(); pi != parameters.end(); ++pi)
	delete pi->second;
}


void DSMSensor::addSampleTag(SampleTag* val)
    throw(atdUtil::InvalidParameterException)
{
    sampleTags.insert(val);
    constSampleTags.insert(val);
}

SampleTagIterator DSMSensor::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator DSMSensor::getVariableIterator() const
{
    return VariableIterator(this);
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

void DSMSensor::setHeight(const string& val)
{
    heightString = val;
    if (heightString.length() > 0) {
	float h;
	istringstream ist(val);
	ist >> h;
	if (ist.fail()) height = floatNAN;
	else if (!ist.eof()) {
	    string units;
	    ist >> units;
	    if (!ist.fail()) {
		if (units == "cm") h /= 10.0;
		else if (units != "m") h = floatNAN;
	    }
	    height = h;
	}
	setSuffix(string(".") + heightString + getSiteSuffix());
    }
    else {
	height = floatNAN;
	setSuffix(getSiteSuffix());
    }
}

void DSMSensor::setHeight(float val)
{
    height = val;
    if (! isnan(height)) {
	ostringstream ost;
	ost << height << 'm';
	heightString = ost.str();
	setSuffix(string(".") + heightString + getSiteSuffix());
    }
    else setSuffix(getSiteSuffix());
}

void DSMSensor::setDepth(const string& val)
{
    depthString = val;
    if (depthString.length() > 0) {
	float d;
	istringstream ist(val);
	ist >> d;
	if (ist.fail()) height = floatNAN;
	else if (!ist.eof()) {
	    string units;
	    ist >> units;
	    if (!ist.fail()) {
		if (units == "cm") d /= 10.0;
		else if (units != "m") d = floatNAN;
	    }
	    height = -d;
	}
	setSuffix(string(".") + depthString + getSiteSuffix());
    }
    else {
	height = floatNAN;
	setSuffix(getSiteSuffix());
    }
}

void DSMSensor::setDepth(float val)
{
    height = -val;
    if (! isnan(height)) {
	ostringstream ost;
	ost << val * 10.0 << "cm";
	depthString = ost.str();
	setSuffix(string(".") + depthString + getSiteSuffix());
    }
    else setSuffix(getSiteSuffix());
}

/*
 * Add a parameter to my map, and list.
 */
void DSMSensor::addParameter(Parameter* val)
{
    map<string,Parameter*>::iterator pi = parameters.find(val->getName());
    if (pi == parameters.end()) {
        parameters[val->getName()] = val;
	constParameters.push_back(val);
    }
    else {
	// parameter with name exists. If the pointers aren't equal
	// delete the old parameter.
	Parameter* p = pi->second;
	if (p != val) {
	    // remove it from constParameters list
	    list<const Parameter*>::iterator cpi = constParameters.begin();
	    for ( ; cpi != constParameters.end(); ) {
		if (*cpi == p) cpi = constParameters.erase(cpi);
		else ++cpi;
	    }
	    delete p;
	    pi->second = val;
	    constParameters.push_back(val);
	}
    }
}

const Parameter* DSMSensor::getParameter(const std::string& name) const
{
    map<string,Parameter*>::const_iterator pi = parameters.find(name);
    if (pi == parameters.end()) return 0;
    return pi->second;
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
    const Site* site = getDSMConfig()->getSite();

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    if (attr.getName() == "devicename")
		setDeviceName(attr.getValue());
	    // set the catalog name from the ID
	    else if (attr.getName() == "ID")
		setCatalogName(attr.getValue());
	    else if (attr.getName() == "class") {
	        if (getClassName().length() == 0)
		    setClassName(attr.getValue());
		else if (getClassName() != attr.getValue())
		    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    	"class attribute=%s does not match getClassName()=%s\n",
			attr.getValue().c_str(),getClassName().c_str());
	    }
	    else if (attr.getName() == "location")
		setLocation(attr.getValue());
	    else if (attr.getName() == "id") {
		istringstream ist(attr.getValue());
		// If you unset the dec flag, then a leading '0' means
		// octal, and 0x means hex.
		ist.unsetf(ios::dec);
		unsigned long val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("sensor",
		    	attr.getName(),attr.getValue());
		setShortId(val);
	    }
	    else if (attr.getName() == "latency") {
		istringstream ist(attr.getValue());
		float val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("sensor",
		    	attr.getName(),attr.getValue());
		setLatency(val);
	    }
	    else if (attr.getName() == "height")
	    	setHeight(attr.getValue());
	    else if (attr.getName() == "depth")
	    	setDepth(attr.getValue());
	    else if (attr.getName() == "suffix")
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

	if (elname == "sample") {
	    SampleTag* newtag = new SampleTag();
	    newtag->setDSM(getDSMConfig());
	    newtag->setDSMId(getDSMConfig()->getId());
	    newtag->setSensorId(getShortId());
	    newtag->fromDOMElement((DOMElement*)child);

	    if (newtag->getSampleId() == 0) {
		delete newtag;
		throw atdUtil::InvalidParameterException(
		    getName(),"sample id invalid or not found","0");
	    }

	    set<SampleTag*>& stags = getncSampleTags();
	    set<SampleTag*>::const_iterator si = stags.begin();
	    for ( ; si != stags.end(); ++si) {
		SampleTag* stag = *si;
		// If a sample id matches a previous one (most likely
		// from the catalog) then update it from this DOMElement.
		if (stag->getSampleId() == newtag->getSampleId()) {
		    // update the sample with the new DOMElement
		    stag->setDSMId(getDSMConfig()->getId());
		    stag->setSensorId(getShortId());

		    stag->fromDOMElement((DOMElement*)child);
		    
		    delete newtag;
		    newtag = 0;
		    break;
		}
	    }
	    if (newtag) addSampleTag(newtag);
	}
	else if (elname == "parameter") {
	    Parameter* parameter =
	    Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
    }

    rawSampleTag = new SampleTag();
    rawSampleTag->setSampleId(0);
    rawSampleTag->setSensorId(getShortId());
    rawSampleTag->setDSMId(getDSMConfig()->getId());
    rawSampleTag->setDSM(getDSMConfig());

    if (getSuffix().length() > 0)
    	rawSampleTag->setSuffix(getSuffix());
    if (site) rawSampleTag->setStation(site->getNumber());

    // sensors in the catalog may not have any sample tags
    // so at this point it is OK if sampleTags.size() == 0.

    // Check that sample ids are unique for this sensor.
    // Estimate the rate of the raw sample as the sum of
    // the rates of the processed samples.
    float rawRate = 0.0;
    set<unsigned long> ids;
    set<SampleTag*>& stags = getncSampleTags();
    set<SampleTag*>::const_iterator si = stags.begin();
    for ( ; si != stags.end(); ++si) {
	SampleTag* stag = *si;

	stag->setSensorId(getShortId());
	if (getSuffix().length() > 0)
	    stag->setSuffix(getSuffix());
	if (site) stag->setStation(site->getNumber());

	if (getShortId() == 0) throw atdUtil::InvalidParameterException(
	    	getName(),"id","zero or missing");

	pair<set<unsigned long>::const_iterator,bool> ins =
		ids.insert(stag->getId());
	if (!ins.second) {
	    ostringstream ost;
	    ost << stag->getId();
	    throw atdUtil::InvalidParameterException(
	    	getName(),"duplicate sample id", ost.str());
	}
	rawRate += stag->getRate();
    }
    rawSampleTag->setRate(rawRate);

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

