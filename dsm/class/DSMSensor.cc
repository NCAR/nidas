
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSensor.h $
 ********************************************************************

*/

#include <DSMSensor.h>
#include <ClockSensor.h>

#include <dsm_sample.h>
#include <SamplePool.h>

#include <iostream>
#include <string>
#include <set>

using namespace std;
using namespace dsm;
using namespace xercesc;

DSMSensor::DSMSensor() :
    classname("unknown"),devname("unknown"),id(0),
    BUFSIZE(8192),buffer(0),bufhead(0),buftail(0),samp(0)

{
    initStatistics();
}


DSMSensor::~DSMSensor()
{
    delete [] buffer;
    for (list<SampleTag*>::const_iterator si = sampleTags.begin();
    	si != sampleTags.end(); ++si) delete *si;
}

void DSMSensor::addSampleTag(SampleTag* tag)
	throw(atdUtil::InvalidParameterException)
{
    sampleTags.push_back(tag);
    constSampleTags.push_back(tag);
}

void DSMSensor::initBuffer() throw()
{
    bufhead = buftail = 0;
    delete [] buffer;
    buffer = new char[BUFSIZE];
}

void DSMSensor::destroyBuffer() throw()
{
    delete [] buffer;
    buffer = 0;
}

dsm_sample_time_t DSMSensor::readSamples()
	throw (SampleParseException,atdUtil::IOException)
{
    size_t len = BUFSIZE - bufhead;	// length to read
    size_t rlen;			// read result
    dsm_sample_time_t tt = maxValue(tt);

    rlen = read(buffer+bufhead,len);
    bufhead += rlen;

    // process all data in buffer, pass samples onto clients
    for (;;) {
        if (samp) {
	    rlen = bufhead - buftail;	// bytes available in buffer
	    len = sampDataToRead;	// bytes left to fill sample
	    if (rlen < len) len = rlen;
	    memcpy(sampDataPtr,buffer+buftail,len);
	    buftail += len;
	    sampDataPtr += len;
	    sampDataToRead -= len;
	    if (!sampDataToRead) {		// done with sample
		tt = samp->getTimeTag();	// return last time tag read
		distributeRaw(samp);
		nsamples++;
		if (isClock()) {
		    ClockSensor* clksens = (ClockSensor*) this;
		    Sample* clksamp = clksens->processClockSample(samp);
		    distributeRaw(clksamp);
		}
		samp = 0;
		// Finished with sample. Check for more data in buffer
	    }
	    else break;		// done with buffer
	}
	// Read the header of the next sample
        if (bufhead - buftail <
		(signed)(len = SIZEOF_DSM_SAMPLE_HEADER))
		break;

	struct dsm_sample header;	// temporary header to read into
	memcpy(&header,buffer+buftail,len);
	buftail += len;

	len = header.length;
	samp = getSample<char>(len);
	samp->setTimeTag(header.timetag);
	samp->setDataLength(len);
	samp->setId(getId());	// set sample id to id of this sensor
	sampDataPtr = (char*) samp->getVoidDataPtr();
	sampDataToRead = len;

	// keeps some stats
	if(len < minSampleLength[currStatsIndex]) 
	    minSampleLength[currStatsIndex] = len;
	if (len > maxSampleLength[currStatsIndex])
	    maxSampleLength[currStatsIndex] = len;
    }

    // shift data down. There shouldn't be much - less than a header's worth.
    register char* bp;
    for (bp = buffer; buftail < bufhead; ) 
    	*bp++ = *(buffer + buftail++);

    bufhead = bp - buffer;
    buftail = 0;
    return tt;
}

bool DSMSensor::receive(const Sample *samp)
  	throw(SampleParseException, atdUtil::IOException)
{
    list<const Sample*> results;
    process(samp,results);
    distribute(results);
    return true;
}


/**
 * Default implementation of process just passes samples on.
 */
bool DSMSensor::process(const Sample* s, list<const Sample*>& result)
    	throw(dsm::SampleParseException,atdUtil::IOException)
{
    s->holdReference();
    result.push_back(s);
    return true;
}



void DSMSensor::initStatistics()
{
    currStatsIndex = reportStatsIndex = 0;
										
    sampleRateObs = 0.0;
										
    maxSampleLength[0] = maxSampleLength[1] = 0;
    minSampleLength[0] = minSampleLength[1] = 999999999;
    readErrorCount[0] = readErrorCount[1] = 0;
    writeErrorCount[0] = writeErrorCount[1] = 0;
    nsamples = 0;
    initialTimeSecs = time(0);
}

void DSMSensor::calcStatistics(unsigned long periodMsec)
{
    reportStatsIndex = currStatsIndex;
    currStatsIndex = (currStatsIndex + 1) % 2;
    maxSampleLength[currStatsIndex] = 0;
    minSampleLength[currStatsIndex] = 999999999;
										
    // periodMsec is in milliseconds, hence the factor of 1000.
    sampleRateObs = (float)nsamples / periodMsec * 1000.;
										
    nsamples = 0;
    readErrorCount[0] = writeErrorCount[0] = 0;
}


float DSMSensor::getObservedSamplingRate() const {
  if (reportStatsIndex == currStatsIndex)
      return (float)nsamples/(time(0) - initialTimeSecs);
  else return sampleRateObs;
}

void DSMSensor::fromDOMElement(const DOMElement* node)
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
	    if (!attr.getName().compare("devicename"))
		setDeviceName(attr.getValue());
	    else if (!attr.getName().compare("class"))
		setClassName(attr.getValue());
	    else if (!attr.getName().compare("id")) {
		istringstream ist(attr.getValue());
		// If you unset the dec flag, then a leading '0' means
		// octal, and 0x means hex.
		ist.unsetf(ios::dec);
		unsigned short val;
		ist >> val;
		if (ist.fail() || val < 0)
		    throw atdUtil::InvalidParameterException("sensor","id",
		    	attr.getValue());
		setId(val);
	    }
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
	    SampleTag* samp = new SampleTag();
	    samp->fromDOMElement((DOMElement*)child);
	    addSampleTag(samp);
	}
    }

    if (sampleTags.size() == 0)
	    throw atdUtil::InvalidParameterException(
		getName() + " has no <sample> tags");

    // Set the sample ids to be the sum of the sensor id and sample id
    // Also check that sample ids are unique for this sensor.
    set<unsigned short> ids;
    for (list<SampleTag*>::const_iterator si = sampleTags.begin();
    	si != sampleTags.end(); ++si) {
	SampleTag* samp = *si;

	pair<set<unsigned short>::const_iterator,bool> ins =
		ids.insert(samp->getId());
	if (!ins.second) {
	    ostringstream ost;
	    ost << samp->getId();
	    throw atdUtil::InvalidParameterException(
	    	getName(),"duplicate sample id", ost.str());
	}

	samp->setId(getId() + samp->getId());
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

