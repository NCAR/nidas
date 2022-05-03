// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*
 * LamsSensor
 */


#include "LamsSensor.h"
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMEngine.h>
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
    DSMSensor(), nAVG(4), nPEAK(1000),
    TAS_level(floatNAN), TASlvl(BELOW), tas(floatNAN),
    tas_step(0), nSKIP(0)
{
}

IODevice* LamsSensor::buildIODevice()
{
    return new UnixIODevice();
}
SampleScanner* LamsSensor::buildSampleScanner()
{
    setDriverTimeTagUsecs(USECS_PER_TMSEC);
    return new DriverSampleScanner();
}

void LamsSensor::fromDOMElement(const xercesc::DOMElement* node)
{
    DSMSensor::fromDOMElement(node);

    const Parameter *p;

    // Get manditory parameter(s)
    p = getParameter("TAS_level");
    if (!p || p->getLength() != 1)
        throw n_u::InvalidParameterException(getName(), "TAS_level","not found or not of length 1");
    TAS_level = (float)p->getNumericValue(0);

    p = getParameter("nAVG");
    if (!p || p->getLength() != 1)
        throw n_u::InvalidParameterException(getName(), "nAVG","not found or not of length 1");
    nAVG  = (int)p->getNumericValue(0);

    p = getParameter("nPEAK");
    if (!p || p->getLength() != 1)
        throw n_u::InvalidParameterException(getName(), "nPEAK","not found or not of length 1");
    nPEAK = (int)p->getNumericValue(0);

    p = getParameter("nSKIP");
    if (!p || p->getLength() != 1)
        throw n_u::InvalidParameterException(getName(), "nSKIP","not found or not of length 1");
    nSKIP = (int)p->getNumericValue(0);
}

bool LamsSensor::process(const Sample* samp,list<const Sample*>& results)
{
    unsigned int len = samp->getDataByteLength();
    const unsigned int   * iAvrg;
    const unsigned short * iPeak;

    if (len == LAMS_SPECTRA_SIZE * 6) {
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
        if (len < sizeof(int)) return false;
        int type = *(const int*) samp->getConstVoidDataPtr();
        if (type == LAMS_SPECAVG_SAMPLE_TYPE) {
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
        else if (type == LAMS_SPECPEAK_SAMPLE_TYPE) {
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

void LamsSensor::open(int flags)
{
    DSMSensor::open(flags);

    // Request that fifo be opened at driver end. Not needed in new driver.
    struct lams_set lams_info;
    lams_info.channel = 1;
    ioctl(LAMS_SET_CHN, &lams_info, sizeof(lams_info));

    ioctl(LAMS_TAS_BELOW, 0, 0);
    ioctl(LAMS_N_AVG,     &nAVG,      sizeof(nAVG));
    ioctl(LAMS_N_PEAKS,   &nPEAK,     sizeof(nPEAK));
    ioctl(LAMS_N_SKIP,   &nSKIP,     sizeof(nSKIP));

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}

void LamsSensor::close()
{
    if (DerivedDataReader::getInstance())
            DerivedDataReader::getInstance()->removeClient(this);
    DSMSensor::close();
}

void LamsSensor::derivedDataNotify(const nidas::core::DerivedDataReader * s)
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

void LamsSensor::printStatus(std::ostream& ostr)
{
    DSMSensor::printStatus(ostr);

    struct lams_status status;

    try {
	ioctl(LAMS_GET_STATUS,&status,sizeof(status));

	ostr << "<td align=left>" << "droppedISRsamples=" << status.missedISRSamples <<
            ", droppedOutSamples=" << status.missedOutSamples << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
}

