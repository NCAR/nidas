// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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

#include "A2D_Serial.h"

#include <nidas/core/CalFile.h>
#include <nidas/util/UTime.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>



using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;
using nidas::util::LogScheme;


NIDAS_CREATOR_FUNCTION_NS(raf, A2D_Serial)

A2D_Serial::A2D_Serial() :
    SerialSensor(),
    _nVars(0), _sampleRate(0), _deltaT(0),
    _calFile(0), _outputMode(Engineering), _havePPS(false),
    _shortPacketCnt(0), _badCkSumCnt(0), _largeTimeStampOffset(0)
{
headerLines = 0;
    for (int i = 0; i < getMaxNumChannels(); ++i)
    {
        _gains[i] = 0;          // 1 or 2 is all we support at this time.
        _bipolars[i] = true;    // At this time that is all this device supports.
    }
}

A2D_Serial::~A2D_Serial()
{
    cout << "Number of header lines = " << headerLines << endl;
}


void A2D_Serial::open(int flags) throw(n_u::IOException)
{
    SerialSensor::open(flags);

    readConfig();
}

void A2D_Serial::readConfig() throw(n_u::IOException)
{
    int nsamp = 0;
    bool done = false;

    write("#PCFG\n", 6);        // request config.

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    while (!done) {
        try {
            readBuffer(1 * MSECS_PER_SEC);

            // process all samples in buffer
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {

                distributeRaw(samp);        // send it on to the clients

                nsamp++;
                const char* msg = (const char*) samp->getConstVoidDataPtr();
                if (strstr(msg, "!EOC")) done = true;

            }
            if (nsamp > 50) {
                WLOG(("%s: A2D_Serial open(): expected !EOC, not received",
                            getName().c_str()));
                done = true;
            }
        }
        catch (const n_u::IOTimeoutException& e) {
            throw e;
        }
    }

}

void A2D_Serial::validate() throw(n_u::InvalidParameterException)
{
    SerialSensor::validate();

    const std::list<SampleTag*>& tags = getSampleTags();
    std::list<SampleTag*>::const_iterator ti = tags.begin();
printf("--------------- Validate ----------------\n");
    for ( ; ti != tags.end(); ++ti) {
        SampleTag* stag = *ti;

        // sample 1 is the header packet and is handled by the asciiScanfer
        if (stag->getSampleId() < 2)
            continue;


        const std::list<const Parameter*>& params = stag->getParameters();
        list<const Parameter*>::const_iterator pi;
        for (pi = params.begin(); pi != params.end(); ++pi) {
            const Parameter* param = *pi;
            const string& pname = param->getName();
            if (pname == "outputmode") {
                    if (param->getType() != Parameter::STRING_PARAM ||
                        param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),"sample",
                            "output mode parameter is not a string");
                    string fname = param->getStringValue(0);
                    if (fname == "counts") _outputMode = Counts;
                    else if (fname == "volts") _outputMode = Volts;
                    else if (fname == "engineering") _outputMode = Engineering;
                    else throw n_u::InvalidParameterException(getName(),"sample",
                            fname + " output mode is not supported");
            }
        }

        const vector<Variable*>& vars = stag->getVariables();
        _nVars = stag->getVariables().size();

        for (int iv = 0; iv < _nVars; iv++) {
            Variable* var = vars[iv];

            int ichan = iv;
            int fgain = 0;
            int bipolar = true;

            const std::list<const Parameter*>& vparams = var->getParameters();
            list<const Parameter*>::const_iterator pi;
            for (pi = vparams.begin(); pi != vparams.end(); ++pi) {
                const Parameter* param = *pi;
                const string& pname = param->getName();
                if (pname == "gain") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),
                            pname,"no value");

                    fgain = param->getNumericValue(0);
                }
                else if (pname == "bipolar") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),
                            pname,"no value");
                    bipolar = param->getNumericValue(0) != 0;
                }
/*
 * Currently channels must be consecutive, no gaps.  If you want to be able to
 * specify channel number then engage this and modilfy code to have list of
 * channel #'s.  As is, you may skip a channel by calling it DUMMY and parsing
 * it.
                else if (pname == "channel") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),pname,"no value");
                    ichan = (int)param->getNumericValue(0);
                }
*/
            }

            _gains[ichan] = fgain;
            _bipolars[ichan] = bipolar;

// @TODO Need to set gain/offset/cals if read in....
        }
    }

printf("OutputMode = %d\n", (int)_outputMode);
for (int i = 0; i < _nVars; ++i)
{
  printf("gain=%d, offset=%d, cals=", _gains[i], _bipolars[i]);
  for (int j = 0; j < _polyCals[i].size(); ++j)
    printf("%f, ", _polyCals[i].at(j));
  printf("\n");
}
printf("------------- End Validate --------------\n");
}

void A2D_Serial::init() throw(n_u::InvalidParameterException)
{
    CharacterSensor::init();

    // Determine number of floats we will recieve (_noutValues)
    list<SampleTag*>& stags = getSampleTags();
    if (stags.size() != 2)
        throw n_u::InvalidParameterException(getName(),"sample",
              "must be two <sample> tags for this sensor");


    _sampleRate = stags.back()->getRate();
    _deltaT = (int)rint(USECS_PER_SEC / _sampleRate);

    const map<string,CalFile*>& cfs = getCalFiles();
    // Just use the first file. If, for some reason a second calibration
    // is applied to this sensor, we must differentiate them by name.
    // Note this calibration is separate from that applied to each variable.
    if (!cfs.empty()) _calFile = cfs.begin()->second;
}

bool A2D_Serial::checkCkSum(const Sample * samp, const char *data)
{
    bool rc = false;

    char *pos = ::strrchr((char *)data, ',');
    if (pos == 0)
    {
        WLOG(("%s: short SerialAnalog packet at ",getName().c_str()) <<
            n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") << ", #bad=" << ++_shortPacketCnt);
        return false;   // No comma's?  Can't be valid.
    }

    // Generate a checksum
    uint16_t cksum = 0;
    int nbytes = pos - data; // sum through last comma
    for (int i = 0; i < nbytes; ++i) cksum += data[i];
    cksum &= 0x00FF;
    ++pos; // move past comma

    // Extract and compare with checksum sent.
    unsigned int ckSumSent;
    sscanf((const char *)pos, "%x", &ckSumSent);
    rc = (cksum == ckSumSent);
    if (rc == false)
            WLOG(("%s: bad SerialAnalog checksum at ", getName().c_str()) <<
                n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") <<
                ", #bad=" << ++_badCkSumCnt);

    return rc;
}

bool A2D_Serial::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    const char *cp = (const char*)samp->getConstVoidDataPtr();

    // Process non-data lines (i.e. process header).
    if (cp[0] != '#')
    {
        // Decode the data with the standard ascii scanner.
        bool rc = SerialSensor::process(samp, results);
        if (results.empty()) return false;
++headerLines;
        // Extract PPS/IRIG status from 'H' header values.
        list<const Sample *>::const_iterator it = results.begin();
        for (; it != results.end(); ++it)
        {
            Sample * nco_samp = const_cast<Sample *>(*it);
            float *values = (float *)nco_samp->getVoidDataPtr();
            // Skip housekeeping; sample id 1.
            if ((nco_samp->getId() - getId()) == 1) {
                if (((int)values[2] & 0x03) < 2)
                    _havePPS = false;
                else
                    _havePPS = true;
            }
        }

        return rc;
    }


    /*
     * Process data-lines.  We should have otherwise returned.
     */

    // copy off data and null-terminate.
    int nbytes = samp->getDataByteLength();
    char input[nbytes+1];
    ::memcpy(input, cp, nbytes);
    input[nbytes] = 0;

    if (checkCkSum(samp, input) == false) return false;

    SampleT<float>* outs = getSample<float>(_nVars);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 2);

    int hz_counter;
    sscanf(input, "#%x,", &hz_counter);

    if (_havePPS)  // Use DSM timestamp if no PPS (i.e. do nothing).
    {
        /* these two variables are microsecond offsets from the start of the second.
         * - usec is the microseconds from the DSM timestamp.
         * - offset is the manufactured usec we want to use.
         * If everything is working correctly then the diff of these two should
         * be a few milliseconds.  But chrony/ntp on the DSM could drift some
         * or the DSM could be burdened with other work, then the diff might
         * creep up into 10's of milliseconds.
         */
        long offset = hz_counter * _deltaT;
        long usec = samp->getTimeTag() % USECS_PER_SEC;


        if (abs(usec - offset) > 900000) // 900 msec - adjust samples in wrong second
        {
            // late samples whose timetag is in next sec are actually from previous second
            if (usec > 900000) offset += USECS_PER_SEC;

            // Reverse sitution if analog clock is a bit slow
            if (usec < 100000) offset -= USECS_PER_SEC;
        }

        dsm_time_t timeoffix = samp->getTimeTag() - usec + offset;
        outs->setTimeTag(timeoffix);
    }



    readCalFile(samp->getTimeTag());    // A2D Cals
    float * dout = outs->getDataPtr();
    int data;
    const char *p = ::strchr(cp, ',');
    for (int ival = 0; ival < _nVars && p; ival++)
    {
        cp = p + 1;
        p = ::strchr(cp, ',');

        if (sscanf(cp, "%x", &data) == 1)
            dout[ival] = float(data);
        else
            dout[ival] = float(NAN);

// Apply A2D cals here.
    }

    if (_outputMode == Engineering) {
        list<SampleTag*> tags = getSampleTags(); tags.pop_front();
        applyConversions(tags.front(), outs);
    }
    results.push_back(outs);

    return true;
}


void A2D_Serial::readCalFile(dsm_time_t tt) throw()
{
    if (!_calFile) return;

    if (getOutputMode() == Counts)
        return;

    // Read CalFile  containing the following fields after the time
    // gain bipolar(1=true,0=false) intcp0 slope0 intcp1 slope1 ... intcp7 slope7

    while (tt >= _calFile->nextTime().toUsecs()) {
        int nd = 2 + getMaxNumChannels() * 2;
        float d[nd];
        try {
            n_u::UTime calTime;
            int n = _calFile->readCF(calTime, d,nd);
            if (n < 2) continue;
            int cgain = (int)d[0];
            int cbipolar = (int)d[1];
            for (int i = 0;
                i < std::min((n-2)/2,getMaxNumChannels()); i++) {
                    int gain = getGain(i);
                    int bipolar = getBipolar(i);
                    if ((cgain < 0 || gain == cgain) &&
                        (cbipolar < 0 || bipolar == cbipolar))
                        setConversionCorrection(i, &d[2+i*4], 4);
            }
        }
        catch(const n_u::EOFException& e)
        {
        }
        catch(const n_u::IOException& e)
        {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                _calFile->getCurrentFileName().c_str(),e.what());
            _calFile = 0;
            break;
        }
        catch(const n_u::ParseException& e)
        {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                _calFile->getCurrentFileName().c_str(),e.what());
            _calFile = 0;
            break;
        }
    }
}

void A2D_Serial::setConversionCorrection(int ichan, const float d[],
    int n) throw(n_u::InvalidParameterException)
{
    _polyCals[ichan].clear();
    if (getOutputMode() == Counts) {
// I think we can just apply nothing if size() is zero...
//        _polyCals[ichan].push_back(0.0);
//        _polyCals[ichan].push_back(1.0);
        return;
    }

    // Strip off trailing zeroes.
    for (int i = n-1; i > 0; --i)
        if (d[i] == 0.0) --n;

    if (n == 2 && d[0] == 0.0 && d[1] == 1.0)
        return;

    for (int i = 0; i < n; ++i)
        _polyCals[ichan].push_back(d[i]);
}

int A2D_Serial::getGain(int ichan) const
{
    if (ichan < 0 || ichan >= getMaxNumChannels()) return 0;
    return _gains[ichan];
}

int A2D_Serial::getBipolar(int ichan) const
{
    if (ichan < 0 || ichan >= getMaxNumChannels()) return -1;
    return _bipolars[ichan];
}

