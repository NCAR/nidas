
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

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
// #include <xercesc/dom/DOMAttr.hpp>

#include <iostream>
#include <string>

using namespace std;
using namespace dsm;
using namespace xercesc;

DSMSensor::DSMSensor()
{
    initStatistics();
}

DSMSensor::DSMSensor(const std::string& n) : devname(n)
{
    initStatistics();
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

