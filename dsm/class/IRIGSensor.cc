/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2005-01-03 13:26:59 -0700 (Mon, 03 Jan 2005) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSerialSensor.cc $

 ******************************************************************
*/

#include <irigclock.h>
#include <pc104sg.h>
#include <IRIGSensor.h>
#include <RTL_DevIoctlStore.h>

#include <iostream>
#include <sstream>

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_ENTRY_POINT(IRIGSensor)

IRIGSensor::IRIGSensor()
{
}

IRIGSensor::~IRIGSensor() {
    try {
	close();
    }
    catch(atdUtil::IOException& ioe) {
      cerr << ioe.what() << endl;
    }
}
void IRIGSensor::open(int flags) throw(atdUtil::IOException)
{
    cerr << "IRIGSensor::open" << endl;
  
    // It's magic, we can do an ioctl before the device is open!
    ioctl(IRIG_OPEN,(const void*)0,0);
    cerr << "IRIG_OPEN done" << endl;

    RTL_DSMSensor::open(flags);

    int status;
    ioctl(IRIG_GET_STATUS,&status,sizeof(int));
    cerr << "IRIG_GET_STATUS=" << hex << status << dec << endl;

    struct timeval tv;
    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;

    gettimeofday(&tv,0);
    ioctl(IRIG_SET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_SET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;

    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;
}

void IRIGSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing IRIG_CLOSE" << endl;
    ioctl(IRIG_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

void IRIGSensor::fromDOMElement(
	const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    RTL_DSMSensor::fromDOMElement(node);
}

DOMElement* IRIGSensor::toDOMParent(
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

DOMElement* IRIGSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


const Sample* IRIGSensor::process(const Sample* samp)
	throw(atdUtil::IOException,dsm::SampleParseException)
{
    return samp;
}

