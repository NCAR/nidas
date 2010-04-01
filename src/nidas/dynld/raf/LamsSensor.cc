/* 
  LamsSensor
  Copyright 2007 UCAR, NCAR, All Rights Reserved
 
   Revisions:
     $LastChangedRevision:  $
     $LastChangedDate:  $
     $LastChangedBy:  $
     $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsSensor.cc $
*/


// #include <nidas/rtlinux/lams.h>
#include <nidas/dynld/raf/LamsSensor.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,LamsSensor)

LamsSensor::LamsSensor() :
    DSMSensor(), nAVG(20), nPEAK(1000),
    TAS_level(floatNAN), TASlvl(BELOW), tas(floatNAN), tas_step(0) {}

IODevice* LamsSensor::buildIODevice() throw(n_u::IOException)
{
    if (DSMEngine::getInstance()->isRTLinux())
        return new RTL_IODevice();
    else return new UnixIODevice();
}
SampleScanner* LamsSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    if (DSMEngine::getInstance()->isRTLinux())
        setDriverTimeTagUsecs(USECS_PER_MSEC);
    else
        setDriverTimeTagUsecs(USECS_PER_TMSEC);
    return new DriverSampleScanner();
}
  
  	
void LamsSensor::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSensor::fromDOMElement(node);

    const Parameter *p;

    // Get manditory parameter(s)
    p = getParameter("TAS_level");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "TAS_level","not found");
    if (p) TAS_level = (float)p->getNumericValue(0);

    // Get optional parameter(s)
    p = getParameter("nAVG");
    if (p) nAVG  = (unsigned int)p->getNumericValue(0);
    p = getParameter("nPEAK");
    if (p) nPEAK = (unsigned int)p->getNumericValue(0);
}

bool LamsSensor::process(const Sample* samp,list<const Sample*>& results) throw()
{

    unsigned int len = samp->getDataByteLength();
    const unsigned int   * iAvrg;
    const unsigned short * iPeak;

    if (len == 8 + LAMS_SPECTRA_SIZE * 6) {
        // data from rtlinux driver
        // there are two spectral arrays generated by LAMS: average and peak
        const unsigned char * input = (unsigned char *) samp->getConstVoidDataPtr();
        iAvrg = (const unsigned int*)input;
        iPeak = (const unsigned short*)&input[LAMS_SPECTRA_SIZE * sizeof(int)];

        // allocate sample
        SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE * 2);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(getId() + 1);  

        // extract data from a lamsPort structure
        float * dout = outs->getDataPtr();
        size_t iout;

        for (iout = 0; iout < LAMS_SPECTRA_SIZE; iout++)
          *dout++ = (float)*iAvrg++;

        for (iout = 0; iout < LAMS_SPECTRA_SIZE; iout++)
          *dout++ = (float)*iPeak++;
      
        results.push_back(outs);
    }
    else {
        int type = *(const int*) samp->getConstVoidDataPtr();
        if (type == 0) {
            const struct lams_avg_sample* lams =
                (const struct lams_avg_sample*) samp->getConstVoidDataPtr();
            int nval = (samp->getDataByteLength() - sizeof(int)) / sizeof(int);
            iAvrg = lams->data;

            // allocate sample
            SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE);
            outs->setTimeTag(samp->getTimeTag());
            outs->setId(getId() + 1 + type);  

            // extract data from a lamsPort structure
            float * dout = outs->getDataPtr();
            int iout;

            for (iout = 0; iout < std::min(LAMS_SPECTRA_SIZE,nval); iout++)
              *dout++ = (float)*iAvrg++;
            for ( ; iout < LAMS_SPECTRA_SIZE; iout++) *dout++ = floatNAN;
            results.push_back(outs);

        }
        else if (type == 1) {
            const struct lams_peak_sample* lams =
                (const struct lams_peak_sample*) samp->getConstVoidDataPtr();
            int nval = (samp->getDataByteLength() - sizeof(int)) / sizeof(short);
            iPeak = lams->data;

            // allocate sample
            SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE);
            outs->setTimeTag(samp->getTimeTag());
            outs->setId(getId() + 1 + type);  

            // extract data from a lamsPort structure
            float * dout = outs->getDataPtr();
            int iout;

            for (iout = 0; iout < std::min(LAMS_SPECTRA_SIZE,nval); iout++)
              *dout++ = (float)*iPeak++;
            for ( ; iout < LAMS_SPECTRA_SIZE; iout++) *dout++ = floatNAN;
      
            results.push_back(outs);
        }
    }

    return true;
} 

void LamsSensor::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    // Request that fifo be opened at driver end. Not needed in new driver.
    struct lams_set lams_info;
    lams_info.channel = 1;
    ioctl(LAMS_SET_CHN, &lams_info, sizeof(lams_info));

    ioctl(LAMS_TAS_BELOW, 0, 0);
    ioctl(LAMS_N_AVG,     &nAVG,      sizeof(nAVG));
    ioctl(LAMS_N_PEAKS,   &nPEAK,     sizeof(nPEAK));

    n_u::Logger::getInstance()->log(LOG_NOTICE,"LamsSensor::open(%x)", getReadFd());

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}

void LamsSensor::close() throw(n_u::IOException)
{
    if (DerivedDataReader::getInstance())
            DerivedDataReader::getInstance()->removeClient(this);
    DSMSensor::close();
}

void LamsSensor::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
    // Generate a fake True Heading that changes over time.
    if (tas_step) {
        if (isnan(tas)) tas = 100.0;
        tas += tas_step;
        if (tas >=  200.0) tas_step *= -1;
        if (tas <=  100.0) tas_step *= -1;
    }
    else
        tas = s->getTrueAirspeed();

    if (isnan(tas)) return;

    WLOG(("%s: TASlvl: %d  TAS_level: %5.1f  tas: %5.1f",getName().c_str(),
         TASlvl, TAS_level, tas));

    switch (TASlvl) {
    case BELOW:
        if (tas >= TAS_level) {
            TASlvl = ABOVE;
            ioctl(LAMS_TAS_ABOVE, 0, 0);
            WLOG(("%s: setting TAS_ABOVE",getName().c_str()));
        }
        break;
    case ABOVE:
        if (tas <= TAS_level) {
            TASlvl = BELOW;
            ioctl(LAMS_TAS_BELOW, 0, 0);
            WLOG(("%s: setting TAS_BELOW",getName().c_str()));
        }
        break;
    }
}

void LamsSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct lams_status status;

    try {
	ioctl(LAMS_GET_STATUS,&status,sizeof(status));

	ostr << "<td align=left>" << "droppedISRsamples=" << status.missedISRSamples <<
            ", droppedOutSamples=" << status.missedISRSamples << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
}

