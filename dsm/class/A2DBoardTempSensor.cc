/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <A2DBoardTempSensor.h>
#include <a2d_driver.h>

#include <atdUtil/Logger.h>

#include <math.h>

#include <iostream>


using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(A2DBoardTempSensor)

A2DBoardTempSensor::A2DBoardTempSensor() :
    RTL_DSMSensor()
{
}

A2DBoardTempSensor::~A2DBoardTempSensor()
{
}

void A2DBoardTempSensor::open(int flags) throw(atdUtil::IOException)
{
    init();
    cerr << "doing A2D_OPEN_I2CT" << endl;
    ioctl(A2D_OPEN_I2CT,&rate,sizeof(rate));
    RTL_DSMSensor::open(flags);
}
void A2DBoardTempSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing A2D_CLOSE_I2CT" << endl;
    ioctl(A2D_CLOSE_I2CT,(const void*)0,0);
    RTL_DSMSensor::close();
}

void A2DBoardTempSensor::init() throw()
{
    const vector<const SampleTag*>& stags = getSampleTags();
    if (stags.size() == 1) {
        sampleId = stags[0]->getId();
	float rate = stags[0]->getRate();
	rate = irigClockRateToEnum((int)rate);
    }
}

void A2DBoardTempSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    A2D_STATUS stat;
    try {
	ioctl(A2D_STATUS_IOCTL,&stat,sizeof(stat));

	ostr << "<td align=left>" <<
		"#full=" << stat.fifofullctr <<
		", #3/4=" << stat.fifo34ctr <<
		", #1/2=" << stat.fifo24ctr <<
		", #1/4=" << stat.fifo14ctr <<
		", #0/4=" << stat.fifoemptyctr <<
		"</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool A2DBoardTempSensor::process(const Sample* insamp,list<const Sample*>& result) throw()
{
    // number of data values in this raw sample.
    if (insamp->getDataByteLength() / sizeof(short) != 1) return false;

    // pointer to raw I2C value
    const signed short* sp = (const signed short*)
    	insamp->getConstVoidDataPtr();

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(sampleId);
    osamp->getDataPtr()[0] = *sp / 16.0;

    result.push_back(osamp);
    return true;
}


