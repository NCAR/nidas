
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
#include <XMLStringConverter.h>
#include <XDOM.h>

#include <dsm_sample.h>
#include <SamplePool.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
// #include <xercesc/dom/DOMAttr.hpp>

#include <iostream>
#include <string>

using namespace std;
using namespace dsm;
using namespace xercesc;

DSMSensor::DSMSensor() :
    BUFSIZE(8192),buffer(0),bufhead(0),buftail(0),samp(0)
{
    initStatistics();
}

DSMSensor::DSMSensor(const std::string& n) : devname(n),
    BUFSIZE(8192),buffer(0),bufhead(0),buftail(0),samp(0)
{
    initStatistics();
}

DSMSensor::~DSMSensor()
{
    delete [] buffer;
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

		process(samp);			// does not freeReference
		
	        distributeRaw(samp);

		samp->freeReference();
		nsamples++;
		samp = 0;			// finished with sample
						// check for more data
						// in buffer
	    }
	    else break;				// done with buffer
	}
	// Read the header of the next sample
        if (bufhead - buftail <
		(signed)(len = SIZEOF_DSM_SAMPLE_HEADER))
		break;

	struct dsm_sample header;	// temporary header to read into
	memcpy(&header,buffer+buftail,len);
	buftail += len;

	len = header.length;
	samp = SamplePool<CharSample>::getInstance()->getSample(len);
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


/**
 * Default implementation of process just distributes this
 * raw sample to my SampleClient's.  readSamples has already
 * distributed the sample to my raw SampleClient's.
 */
void DSMSensor::process(const Sample* s)
    	throw(dsm::SampleParseException,atdUtil::IOException)
{
    distribute(s);
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

    cerr << "DSMSensor::fromDOMElement element name=" <<
    	xnode.getNodeName() << endl;
	
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	cerr <<"\tAttributes" << endl;
	cerr <<"\t----------" << endl;
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    cerr << "attrname=" << attr.getName() << endl;
	    if (!attr.getName().compare("devicename")) {
		setDeviceName(attr.getValue());
		cerr << "\tattrval=" << attr.getValue() << endl;
	    }
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

