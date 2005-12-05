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
#include <iomanip>

using namespace dsm;
using namespace std;

CREATOR_FUNCTION(A2DBoardTempSensor)

A2DBoardTempSensor::A2DBoardTempSensor() :
    RTL_DSMSensor(),rate(IRIG_1_HZ),
    DEGC_PER_CNT(0.0625)
{
}

A2DBoardTempSensor::~A2DBoardTempSensor()
{
}

void A2DBoardTempSensor::open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    init();
    // cerr << "doing A2D_OPEN_I2CT" << endl;
    ioctl(A2D_OPEN_I2CT,&rate,sizeof(rate));
    RTL_DSMSensor::open(flags);
}
void A2DBoardTempSensor::close() throw(atdUtil::IOException)
{
    // cerr << "doing A2D_CLOSE_I2CT" << endl;
    ioctl(A2D_CLOSE_I2CT,(const void*)0,0);
    RTL_DSMSensor::close();
}

float A2DBoardTempSensor::getTemp() throw(atdUtil::IOException)
{
    short tval;
    ioctl(A2D_GET_I2CT,&tval,sizeof(tval));
    return tval * DEGC_PER_CNT;
}

void A2DBoardTempSensor::init() throw(atdUtil::InvalidParameterException)
{
    const vector<const SampleTag*>& stags = getSampleTags();
    if (stags.size() == 1) {
        sampleId = stags[0]->getId();
	rate = irigClockRateToEnum((int)stags[0]->getRate());
    }
}

void A2DBoardTempSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    try {
        float tdeg = getTemp();
	ostr << "<td align=left>" << fixed << setprecision(1) <<
	    tdeg << " degC</td>" << endl;
    }
    catch (const atdUtil::IOException& e) {
	ostr << "<td>" << e.what() << "</td>" << endl;
    }
}

bool A2DBoardTempSensor::process(const Sample* insamp,list<const Sample*>& result) throw()
{
    // number of data values in this raw sample. Should be one.
    if (insamp->getDataByteLength() / sizeof(short) != 1) return false;

    // pointer to 16 bit raw temperature
    const signed short* sp = (const signed short*)
    	insamp->getConstVoidDataPtr();

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(sampleId);
    osamp->getDataPtr()[0] = *sp * DEGC_PER_CNT;

    result.push_back(osamp);
    return true;
}


