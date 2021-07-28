// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#include <nidas/linux/arinc/arinc.h>
#include "DSMArincSensor.h"
#include "UDPArincSensor.h"
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <cmath>
#include <cstdlib>
#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <set>

/**
 * The Alta Enet device gives us the ARINC label in reverse.  Use this lookup
 * table to reverse the bits.
 */
static const unsigned char ReverseBits[] =
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};


using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

DSMArincSensor::DSMArincSensor() :
    _altaEnetDevice(false), _speed(AR_HIGH), _parity(AR_ODD),_converters(),
    _ttadjusters()
{
    for (unsigned int label = 0; label < NLABELS; label++)
    {
        _processed[label] = false;
        _observedLabelCnt[label] = 0;
    }
}

DSMArincSensor::~DSMArincSensor()
{
    for (map<dsm_sample_id_t, TimetagAdjuster*>::const_iterator tti =
            _ttadjusters.begin();
        tti != _ttadjusters.end(); ++tti) {
        TimetagAdjuster* tta = tti->second;
        if (tta) {
            tta->log(nidas::util::LOGGER_INFO, this, true);
            delete tta;
        }
    }

/* Debug output to show all labels that came from a sensor.

    for (unsigned int label = 0; label < NLABELS; label++)
        if (_observedLabelCnt[label] > 0)
            std::cout << getName().c_str() << " (" << getDSMId() << ", " << getSensorId() + label
              << ") \0" << std::oct << label << " : "
              << std::dec << _observedLabelCnt[label] << std::endl;
*/
}

IODevice* DSMArincSensor::buildIODevice() throw(n_u::IOException)
{
    setDriverTimeTagUsecs(USECS_PER_MSEC);
    return new UnixIODevice();
}

SampleScanner* DSMArincSensor::buildSampleScanner()
throw(n_u::InvalidParameterException)
{
    return new DriverSampleScanner();
}

void DSMArincSensor::open(int flags)
    throw(n_u::IOException, n_u::InvalidParameterException)
{
    // If Alta device then open/scanning handled by UDPArincSensor
    if (_altaEnetDevice)
        return;


    DSMSensor::open(flags);

    if (flags == O_WRONLY) return;

    // sort SampleTags by rate then by label
    list<SampleTag*>& tags = getSampleTags();
    set <SampleTag*, SortByRateThenLabel> sortedSampleTags
        ( tags.begin(), tags.end() );

    for (set<SampleTag*>::const_iterator si = sortedSampleTags.begin();
            si != sortedSampleTags.end(); ++si)
    {
        SampleTag* stag = *si;
        arcfg_t arcfg;

        // remove the Sensor ID from the short ID to get the label
        arcfg.label = stag->getSampleId();

        // round down the floating point rates
        arcfg.rate  = (short) floor( stag->getRate() );

        //#define DEBUG
#ifdef DEBUG
        // Note - ARINC samples have only one variable...
        const Variable* var = stag->getVariables().front();

        ILOG(("proc: %s labl: %04o  rate: %2d %6.3f  units: %8s  name: %20s  longname: %s",
                    (stag->isProcessed() ?"Y":"N"), arcfg.label, arcfg.rate, stag->getRate(),
                    (var->getUnits()).c_str(), (var->getName()).c_str(), (var->getLongName()).c_str()));
#endif
        ioctl(ARINC_SET, &arcfg, sizeof(arcfg_t));
    }
    sortedSampleTags.clear();

    archn_t archn;
    archn.speed  = _speed;
    archn.parity = _parity;
    ioctl(ARINC_OPEN, &archn, sizeof(archn_t));
}

void DSMArincSensor::close() throw(n_u::IOException)
{
    DSMSensor::close();
}

/*
 * Initialize anything needed for process method.
 */
void DSMArincSensor::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();

    float ttadjustVal = 0.0;

    const Parameter *parm = getParameter("ttadjust");
    if (parm) {
        if (parm->getType() == Parameter::STRING_PARAM || parm->getLength() < 1)
            throw n_u::InvalidParameterException(getName(),"ttadjust", "is not numeric of length 1");
        if (parm->getLength() > 1)
            WLOG(("%s: ttadjust with more than one value is deprecated, should be single valued: 0 (disable) or 1 (enable)",
                getName().c_str()));
        ttadjustVal = parm->getNumericValue(0);
    }

    list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si;
    for (si = tags.begin(); si != tags.end(); ++si) {
        SampleTag* stag = *si;
        unsigned short label = stag->getSampleId();
        _processed[label] = stag->isProcessed();
        if (stag->isProcessed()) {
            if (getApplyVariableConversions()) {
                for (unsigned int iv = 0; iv < stag->getVariables().size(); iv++) {
                    Variable& var = stag->getVariable(iv);
                    VariableConverter* vcon = var.getConverter();
                    if (vcon) {
                        if (_converters.find(stag->getId()) != _converters.end())
                            throw n_u::InvalidParameterException(getName(),"variable","more than one variable for a sample id, or init() is being called more than once");
                        _converters[stag->getId()] = vcon;
                    }
                }
            }
            float ttval = ttadjustVal;
            /* The default value for stag->getTimetagAdjust() is -1.
             * A value of 0 here means the user wants to override any
             * value set by a ttadjust <parameter> for the sensor.
             */
            if (stag->getTimetagAdjust() >= 0.0) {
                ttval = stag->getTimetagAdjust();
            }
            if (ttval > 0.0) {
                _ttadjusters[stag->getId()] = new TimetagAdjuster(stag->getId(), stag->getRate());
            }
        }
    }
}

void DSMArincSensor::registerWithUDPArincSensor()
{
    // If we are the Alta:Enet device, then locate UDPArincSensor and register ourselves.
    if (_altaEnetDevice && getDeviceName().length() > 5)
    {
        /* Since we fly two of these, we need to know which UDPArincSensor this goes with.
         * This is entered as the refID in the overloaded devicename.
         */
        unsigned int channel, refID;
        sscanf(getDeviceName().c_str(), "Alta:%u:%u", &refID, &channel);

        const std::list<DSMSensor*>& sensors = getDSMConfig()->getSensors();
        for (list<DSMSensor*>::const_iterator si = sensors.begin(); si != sensors.end(); ++si)
        {
            DSMSensor* snsr = *si;
            if (snsr->getClassName().compare("raf.UDPArincSensor") == 0 && snsr->getSensorId() == refID)
            {
                dynamic_cast<UDPArincSensor *>(snsr)->registerArincSensor(channel, this);
                DLOG(( "Registering DSMArincSensor with UDPArincSensor id=%d, channel=%d.", refID, channel ));
            }
        }
    }
}

/**
 * Each label contains it's own time tag in seconds
 * since 00:00 GMT. The input sample's time tag is
 * used to set the date portion of the output sample timetags.
 */
bool DSMArincSensor::process(const Sample* samp,list<const Sample*>& results)
throw()
{
    const tt_data_t *pSamp = (const tt_data_t*) samp->getConstVoidDataPtr();
    int nfields = samp->getDataByteLength() / sizeof(tt_data_t);

    // absolute time at 00:00 GMT of day.
    dsm_time_t t0day = samp->getTimeTag() - (samp->getTimeTag() % USECS_PER_DAY);
    dsm_time_t tt;

    // milliseconds since 00:00 UTC
    int tmodMsec = (samp->getTimeTag() % USECS_PER_DAY) / USECS_PER_MSEC;

    for (int i=0; i<nfields; i++) {

        //     if (i == nfields-1)
        //       ILOG(("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
        //             (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned int)0xffffff00) ));

        unsigned short label = pSamp[i].data & 0xff;
        _observedLabelCnt[label]++;
        //     ILOG(("%3d/%3d %08x %04o", i, nfields, pSamp[i].data, label ));

        // Even if the user doesn't want to see a value (_processed[label] == false),
        // we still want to process it.
        // For example on the Honeywell GPS, latitude and longitude have separate labels
        // for the coarse and fine values. When the label for the fine latitude
        // is found, a derived total latitude is generated from the sum of the coarse and fine
        // values. In this case we want to process the coarse latitude label, but
        // that value is not passed as a sample.
        sampleType stype = FLOAT_ST;
        double d = processLabel(pSamp[i].data,&stype);

        if (!_processed[label]) continue;

        // pSamp[i].time is the number of milliseconds since midnight
        // for the individual label. Use it to create a correct
        // time tag for the label, which is in units of microseconds.

        // On startup the initial pSamp[i].time values can be bad for the first
        // second (e.g. PREDICT tf04). Check the difference between the sample
        // time and pSamp[i].time
        int td = pSamp[i].time - tmodMsec;

        if (::abs(td) < MSECS_PER_SEC) {
            tt = t0day + (dsm_time_t)pSamp[i].time * USECS_PER_MSEC;

            // correct for problems around midnight rollover
            if (::llabs(tt - samp->getTimeTag()) > USECS_PER_HALF_DAY) {
                if (tt > samp->getTimeTag()) tt -= USECS_PER_DAY;
                else tt += USECS_PER_DAY;
            }
        }
        else {
            tt = samp->getTimeTag();
#ifdef DEBUG
            WLOG(("%s: tmodMsec=%d, pSamp[%d].time=%d",
                        n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H%M%S.%4f").c_str(),tmodMsec,i,pSamp[i].time));
#endif
        }

        // sample id is sum of sensor id and label
        dsm_sample_id_t id = getId() + label;

        TimetagAdjuster* ttadj = _ttadjusters[id];
        if (ttadj) tt = ttadj->adjust(tt);

        // if there is a VariableConverter defined for this sample, apply it.
        if (_converters.find(id) != _converters.end())
            d = _converters[id]->convert(tt,d);

        Sample* outs = 0;

        switch (stype) {
        case DOUBLE_ST:
            {
                SampleT<double>* outd = getSample<double>(1);
                outd->getDataPtr()[0] = d;
                outs = outd;
            }
            break;
        case UINT32_ST:
            {
                SampleT<uint32_t>* outi = getSample<uint32_t>(1);
                outi->getDataPtr()[0] = (uint32_t) d;
                outs = outi;
            }
            break;
        case FLOAT_ST:
        default:
            {
                SampleT<float>* outf = getSample<float>(1);
                outf->getDataPtr()[0] = (float) d;
                outs = outf;
            }
            break;
        }

        // set the sample id to sum of sensor id and label
        outs->setId(id);
        outs->setTimeTag(tt);
        results.push_back(outs);
    }

    return true;
}

bool DSMArincSensor::processAlta(const dsm_time_t timeTag, unsigned char *input, int nfields, std::list<const Sample*> &results) throw()
{
    const txp *pSamp = (const txp*) input;

    // absolute time at 00:00 GMT of day.
    dsm_time_t t0day = timeTag - (timeTag % USECS_PER_DAY);
    dsm_time_t tt;

    // milliseconds since 00:00 UTC
    int tmodMsec = (timeTag % USECS_PER_DAY) / USECS_PER_MSEC;

    for (int i = 0; i < nfields; i++) {

        //     if (i == nfields-1)
        //       ILOG(("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
        //             (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned int)0xffffff00) ));

        uint32_t data = pSamp[i].data;
        unsigned short label = ReverseBits[data & 0xff];
        data = (data & 0xffffff00) + label;
        _observedLabelCnt[label]++;

        // Even if the user doesn't want to see a value (_processed[label] == false),
        // we still want to process it.
        // For example on the Honeywell GPS, latitude and longitude have separate labels
        // for the coarse and fine values. When the label for the fine latitude
        // is found, a derived total latitude is generated from the sum of the coarse and fine
        // values. In this case we want to process the coarse latitude label, but
        // that value is not passed as a sample.
        sampleType stype = FLOAT_ST;
        double d = processLabel(data, &stype);

        if (!_processed[label]) continue;

        // pSamp[i].time is the number of milliseconds since midnight
        // for the individual label. Use it to create a correct
        // time tag for the label, which is in units of microseconds.

        // On startup the initial pSamp[i].time values can be bad for the first
        // second (e.g. PREDICT tf04). Check the difference between the sample
        // time and pSamp[i].time
        int td = pSamp[i].time - tmodMsec;
//DLOG(("  td=%d = %d - %d", td, pSamp[i].time, tmodMsec));
        if (::abs(td) < MSECS_PER_SEC) {
            tt = t0day + (dsm_time_t)pSamp[i].time * USECS_PER_MSEC;

            // correct for problems around midnight rollover
            if (::llabs(tt - timeTag) > USECS_PER_HALF_DAY) {
                if (tt > timeTag) tt -= USECS_PER_DAY;
                else tt += USECS_PER_DAY;
            }
        }
        else {
            tt = timeTag;
#ifdef DEBUG
            WLOG(("%s: tmodMsec=%d, pSamp[%d].time=%d",
                        n_u::UTime(timeTag).format(true,"%Y %m %d %H%M%S.%4f").c_str(),tmodMsec,i,pSamp[i].time));
#endif
        }

        DLOG(("%3d/%3d %s %s dt=%llu %04o %08x %f",
            i, nfields,
            n_u::UTime(timeTag).format(true,"%H%M%S.%4f").c_str(),
            n_u::UTime(tt).format(true,"%H%M%S.%4f").c_str(), (timeTag-tt)/1000,
            label, data, d ));

        // sample id is sum of sensor id and label
        dsm_sample_id_t id = getId() + label;

        // if there is a VariableConverter defined for this sample, apply it.
        if (_converters.find(id) != _converters.end())
            d = _converters[id]->convert(tt,d);

        Sample* outs = 0;

        switch (stype) {
        case DOUBLE_ST:
            {
                SampleT<double>* outd = getSample<double>(1);
                outd->getDataPtr()[0] = d;
                outs = outd;
            }
            break;
        case UINT32_ST:
            {
                SampleT<uint32_t>* outi = getSample<uint32_t>(1);
                outi->getDataPtr()[0] = (uint32_t) d;
                outs = outi;
            }
            break;
        case FLOAT_ST:
        default:
            {
                SampleT<float>* outf = getSample<float>(1);
                outf->getDataPtr()[0] = (float) d;
                outs = outf;
            }
            break;
        }

        // set the sample id to sum of sensor id and label
        outs->setId(id);
        outs->setTimeTag(tt);
        results.push_back(outs);
    }

    return true;
}

void DSMArincSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
        ostr << "<td align=left><font color=red><b>not active</b></font></td></tr>" << endl;
        return;
    }

    dsm_arinc_status stat;
    try {
        ioctl(ARINC_STAT,&stat,sizeof(stat));

        ostr << "<td align=left>" <<
            "lps="         << stat.lps_cnt <<
            "/"            << stat.lps <<
            ", poll="      << stat.pollRate << "Hz" <<
            ", overflow="  << stat.overflow <<
            ", underflow=" << stat.underflow <<
            ", nosync=" << stat.nosync <<
            "</td></tr>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td></tr>" << endl;
        n_u::Logger::getInstance()->log(LOG_ERR,
                "%s: printStatus: %s",getName().c_str(),
                ioe.what());
    }
}

void DSMArincSensor::fromDOMElement(const xercesc::DOMElement* node)
throw(n_u::InvalidParameterException)
{
    DSMSensor::fromDOMElement(node);
    XDOMElement xnode(node);

    if (getDeviceName().find("Alta:") == 0) {
        _altaEnetDevice = true;
        _openable = false;          // Do not add to SensorHandler
//  if strlen < 6 then not long enough.  How has Gordon been validating that there is a port #?
    }

    // parse attributes...
    if(node->hasAttributes()) {

        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();

        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));

            // get attribute name
            const string& aname = attr.getName();
            const string& aval = attr.getValue();

            if (!aname.compare("speed")) {
                DLOG(("%s = %s", aname.c_str(), aval.c_str()));
                if (!aval.compare("high"))     _speed = AR_HIGH;
                else if (!aval.compare("low")) _speed = AR_LOW;
                else throw n_u::InvalidParameterException
                    (DSMSensor::getName(),aname,aval);
            }
            else if (!aname.compare("parity")) {
                DLOG(("%s = %s", aname.c_str(), aval.c_str()));
                if (!aval.compare("odd"))       _parity = AR_ODD;
                else if (!aval.compare("even")) _parity = AR_EVEN;
                else throw n_u::InvalidParameterException
                    (DSMSensor::getName(),aname,aval);
            }
        }
    }
    registerWithUDPArincSensor();
}
