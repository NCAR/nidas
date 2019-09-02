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
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <cmath>
#include <cstdlib>
#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <set>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

DSMArincSensor::DSMArincSensor() :
    _altaEnetDevice(false), _speed(AR_HIGH), _parity(AR_ODD),_converters()
{
    if (getDeviceName().find("Alta:") == 0) {
        _altaEnetDevice = true;
//  if strlen < 6 then not long enough.  How has Gordon been validating that there is a port #?
    }

    for (unsigned int label = 0; label < NLABELS; label++)
    {
        _processed[label] = false;
        _labelCnt[label] = 0;
    }
}

DSMArincSensor::~DSMArincSensor() {
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

    list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si;
    for (si = tags.begin(); si != tags.end(); ++si) {
        SampleTag* stag = *si;
        unsigned short label = stag->getSampleId();
        _processed[label] = stag->isProcessed();
        if (stag->isProcessed() && getApplyVariableConversions()) {
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
    }


    // If we are the Alta:Enet device, then locate UDPArincSensor and register ourselves.
    if (getDeviceName().find("Alta:") == 0 && getDeviceName().length() > 5)
    {
        const std::list<DSMSensor*>& sensors = getDSMConfig()->getSensors();
        for (list<DSMSensor*>::const_iterator si = sensors.begin(); si != sensors.end(); ++si) {
            DSMSensor* snsr = *si;
            if (snsr->getClassName().compare("raf.UDPArincSensor") == 0) {
                std::string tmp = getDeviceName().substr(5, std::string::npos);
                int channel = atoi(tmp.c_str());
                dynamic_cast<UDPArincSensor *>(snsr)->registerArincSensor(channel, this);
                DLOG(( "Registering DSMArincSensor with UDPArincSensor, channel=%d.", channel ));
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

    dsm_time_t t0day = timeTag - (timeTag % USECS_PER_DAY);
    dsm_time_t tt;

    // milliseconds since 00:00 UTC
    int tmodMsec = (timeTag % USECS_PER_DAY) / USECS_PER_MSEC;

    for (int i = 0; i < nfields; i++) {

        //     if (i == nfields-1)
        //       ILOG(("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
        //             (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned int)0xffffff00) ));

//        unsigned short label = pSamp[i].data & 0xff;
        uint32_t data = pSamp[i].data;
        unsigned short label = decodeAltaLabel(data & 0xff);
_labelCnt[label]++;
ILOG(("%3d/%3d %08x %04o", i, nfields, data, label ));

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
        ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
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
            "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
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
}

/* -------------------------------------------------------------------- */
uint32_t DSMArincSensor::decodeAltaLabel(uint32_t data)
{
  uint32_t RXPlabel = data & 0x000000FF;
  uint32_t tempLabel = 0;

  tempLabel |= (RXPlabel & 1) << 7;
  tempLabel |= (RXPlabel & 2) << 5;
  tempLabel |= (RXPlabel & 4) << 3;
  tempLabel |= (RXPlabel & 8) << 1;
  tempLabel |= (RXPlabel & 16) >> 1;
  tempLabel |= (RXPlabel & 32) >> 3;
  tempLabel |= (RXPlabel & 64) >> 5;
  tempLabel |= (RXPlabel & 128) >> 7;

  return tempLabel;
}

