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
#include <nidas/core/DSMEngine.h>
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
    _nVars(0), _sampleRate(0), _deltaT(0), _staticLag(0), _boardID(0),
    _haveCkSum(true), _calFile(0), _outputMode(Engineering), _havePPS(false),
    _calset(0), _voltage(-99), configStatus(),
    _shortPacketCnt(0), _badCkSumCnt(0), _largeTimeStampOffset(0),
    headerLines(0)
{
    for (int i = 0; i < getMaxNumChannels(); ++i)
    {
        _ivarByChan[i] = -1;
        _gains[i] = 1;          // 1 or 2 is all we support at this time.
        _ifsr[i] = 0;           // plus/minus 10
        _bipolar[i] = 1;        // Fixed 1 for this device
        _ipol[i] = 0;
    }

    configStatus["PPS"] = 0;
    configStatus["IFSR"] = true;
    configStatus["IPOL"] = true;
    configStatus["ISEL"] = true;
}

A2D_Serial::~A2D_Serial()
{
    cerr << "A2D_Serial: " << getName() << " #header lines=" << headerLines << ", #shortPkts=" << _shortPacketCnt << ", #badCkSums=" << _badCkSumCnt << endl;
}


void A2D_Serial::open(int flags)
{
    SerialSensor::open(flags);

    readConfig();
// @TODO Need to set gain/offset/cals if read in....

    DSMEngine::getInstance()->registerSensorWithXmlRpc(getDeviceName(),this);
}

void A2D_Serial::readConfig()
{
    int nsamp = 0;
    bool done = false;

    write("#RST\n", 5);        // reset device, which will return config.

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    while (!done) {
        try {
            readBuffer(1 * MSECS_PER_SEC);

            // process all samples in buffer
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {

                nsamp++;
                const char* msg = (const char*) samp->getConstVoidDataPtr();
                if (strstr(msg, "!EOC")) done = true;   // last line of config
                parseConfigLine(msg);

                // send it on to the clients and freeReference
                distributeRaw(samp);
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


void A2D_Serial::dumpConfig() const
{
    cout << "A2D_Serial: " << getName() << " configuration:" << endl;
    cout << "Board ID: XML=" << _boardID << ", dev=" << configStatus.find("BID")->second << endl;
    cout << "Sample rate = " << _sampleRate << endl;
    cout << "Checksum enabled = " << _haveCkSum << endl;
    cout << "OutputMode [Counts, Volts, Engineering] = " << _outputMode << endl;
    cout << "Number variables = " << _nVars << endl;
    if (_calFile)
        cout << "CalFileName = " << _calFile->getCurrentFileName() << endl;

    for (int i = 0; i < getMaxNumChannels(); ++i)
    {
        if (_ivarByChan[i] >= 0) {
            cout << "chan=" << i << ", var#=" << _ivarByChan[i] << ", ifsr=" << _ifsr[i] << ", ipol=" << _ipol[i] << ", cals=";
            for (size_t j = 0; j < _polyCals[i].size(); ++j)
                cout << _polyCals[i].at(j) << ", ";
            cout << endl;
        }
    }
}


void A2D_Serial::validate()
{
    SerialSensor::validate();

    const Parameter* param;
    param = getParameter("boardID");
    if (!param) throw n_u::InvalidParameterException(getName(),
          "boardID","not found");
    _boardID = (int)param->getNumericValue(0);


    const std::list<SampleTag*>& tags = getSampleTags();
    std::list<SampleTag*>::const_iterator ti = tags.begin();

    for ( ; ti != tags.end(); ++ti) {
        SampleTag* stag = *ti;

        // sample 1 is the header packet and is handled by the asciiScanfer
        if (stag->getSampleId() < 2)
            continue;

        _sampleRate = stag->getRate();

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

        int prevChan = -1;

        for (int iv = 0; iv < _nVars; iv++) {
            Variable* var = vars[iv];

            int ichan = prevChan + 1;
            int ifsr = 0;
            int ipol = 0;

            const std::list<const Parameter*>& vparams = var->getParameters();
            list<const Parameter*>::const_iterator pi;
            for (pi = vparams.begin(); pi != vparams.end(); ++pi) {
                const Parameter* param = *pi;
                const string& pname = param->getName();
                if (pname == "ifsr") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),
                            pname,"no value");

                    ifsr = param->getNumericValue(0);
                }
                else if (pname == "ipol") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),
                            pname,"no value");
                    ipol = param->getNumericValue(0) != 0;
                }
                else if (pname == "channel") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),pname,"no value");
                    ichan = (int)param->getNumericValue(0);
                }
            }
            if (ichan < 0 || ichan >= getMaxNumChannels()) {
                ostringstream ost;
                ost << "value=" << ichan << " is outside the range 0:" <<
                    (getMaxNumChannels() - 1);
                throw n_u::InvalidParameterException(getName(),
                        "channel",ost.str());
            }


            // cerr << "A2D_Serial: ichan=" << ichan << " ifsr=" << ifsr << " bipolar=" << ipol << endl;
            var->setA2dChannel(ichan);

            if (_ivarByChan[ichan] >= 0)
                WLOG(("%s: channel %d is assigned to more than one variable",getName().c_str(),ichan));
            _ivarByChan[ichan] = iv;
            _ipol[ichan] = ipol;
            _ifsr[ichan] = ifsr;
            _gains[ichan] = ifsr + 1;  // this works for now, will not if more gains are added
            prevChan = ichan;
        }
    }
}

void A2D_Serial::init()
{
    CharacterSensor::init();

    // Determine number of floats we will recieve (_noutValues)
    list<SampleTag*>& stags = getSampleTags();
    if (stags.size() != 2)
        throw n_u::InvalidParameterException(getName(),"sample",
              "must be two <sample> tags for this sensor");


    _deltaT = (int)rint(USECS_PER_SEC / _sampleRate);
    _staticLag = _deltaT;   // This can be affected by FILT.

    const map<string,CalFile*>& cfs = getCalFiles();
    // Just use the first file. If, for some reason a second calibration
    // is applied to this sensor, we must differentiate them by name.
    // Note this calibration is separate from that applied to each variable.
    if (!cfs.empty()) _calFile = cfs.begin()->second;
}


void A2D_Serial::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
        ostr << "<td align=left><font color=red><b>not active</b></font></td></tr>" << endl;
        return;
    }


    ostr << "<td align=left>";
    bool firstPass = true;
    for (map<string,int>::iterator it = configStatus.begin(); it != configStatus.end(); ++it)
    {
        bool iwarn = false;
        if (!firstPass) ostr << ',';

        if ((it->first).compare("BID") == 0) {
            if (it->second != _boardID)
                iwarn = true;
        }
        else
        if ((it->first).compare("PPS") == 0) {
            if (it->second < 2)
                iwarn = true;
        }
        else
        if (it->second == false)
            iwarn = true;

        if (iwarn) ostr << "<font color=red><b>";
        ostr << it->first;
        if (iwarn) ostr << "</b></font>";
        firstPass = false;
    }
    ostr << "</td></tr>";
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
    if (sscanf((const char *)pos, "%x", &ckSumSent) == 1) rc = (cksum == ckSumSent);
    if (!rc)
        WLOG(("%s: bad SerialAnalog checksum at ", getName().c_str()) <<
            n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") <<
            ", #bad=" << ++_badCkSumCnt);
    return rc;
}

bool A2D_Serial::process(const Sample * samp,
                         list < const Sample * >&results)
{
    const char *cp = (const char*)samp->getConstVoidDataPtr();

    // Process non-data lines (i.e. process header).
    if (cp[0] == '!')   // Startup device configuration.
    {
        parseConfigLine(cp);
        return false;
    }
    if (cp[0] != '#')   // Header line at start of each second.
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

    // Since a sample for this sensor has a scanfFormat, a null char
    // is appended at acquisition time.  Therefore the
    // following memcpy and null termination should be unnecessary.
    // However, in case some data has been taken without null termination
    // we'll leave it in.
    int nbytes = samp->getDataByteLength();
    char input[nbytes+1];
    ::memcpy(input, cp, nbytes);
    input[nbytes] = 0;

    if (_haveCkSum && checkCkSum(samp, input) == false) return false;

    SampleT<float>* outs = getSample<float>(_nVars);
    dsm_time_t ttag = samp->getTimeTag() - getLagUsecs() - _staticLag;
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

        dsm_time_t timeoffix = ttag - usec + offset;
        outs->setTimeTag(timeoffix);
    }
    else
    {   // apply standard
        outs->setTimeTag(ttag);
    }

    readCalFile(ttag);    // A2D Cals
    float * dout = outs->getDataPtr();

    // initialize dout
    for (int iv = 0; iv < _nVars; iv++) dout[iv] = float(NAN);

    int data, ichan;
    const char *p = ::strchr(input, ',');
    for (ichan = 0; p && ichan < getMaxNumChannels(); ichan++)
    {
        cp = p + 1;
        p = ::strchr(cp, ',');

        int iv = _ivarByChan[ichan]; // variable index for this channel

        // This skips a channel we are not using/decoding
        if (iv < 0)
            continue;
        assert(iv < _nVars);

        if (sscanf(cp, "%x", &data) == 1)
            dout[iv] = applyCalibration(float(data), _polyCals[ichan]);
    }

    if (_outputMode == Engineering) {
        list<SampleTag*> tags = getSampleTags(); tags.pop_front();
        applyConversions(tags.front(), outs);
    }
    results.push_back(outs);

    return true;
}


void A2D_Serial::parseConfigLine(const char *data)
{
    int channel, value;

    // data is terminated with a NULL char

    if (sscanf(data, "!OCHK %d", &value) == 1) {
        _haveCkSum = value != 0;
        return;
    }

    if (sscanf(data, "!BID=%d", &value) == 1) {
        configStatus["BID"] = value;
        return;
    }
    if (sscanf(data, "!IFSR_%d=%d", &channel, &value) == 2) {
        if (samplingChannel(channel) && _ifsr[channel] != value) {
            configStatus["IFSR"] = false;
            WLOG(("%s: SerialA2D config mismatch: ifsr chan=%d: xml=%d != dev=%d",
                getName().c_str(), channel, _ifsr[channel], value ));
        }
        return;
    }
    if (sscanf(data, "!IPOL_%d=%d", &channel, &value) == 2) {
        if (samplingChannel(channel) && _ipol[channel] != value) {
            configStatus["IPOL"] = false;
            WLOG(("%s: SerialA2D config mismatch: ipol chan=%d: xml=%d != dev=%d",
                getName().c_str(), channel, _ipol[channel], value ));
        }
        return;
    }
    if (sscanf(data, "!ISEL_%d=%d", &channel, &value) == 2) {
        if (samplingChannel(channel) && value != 0) {
            configStatus["ISEL"] = false;
            WLOG(("%s: SerialA2D ISEL not default: chan=%d val=%d",
                getName().c_str(), channel, value ));
        }
        return;
    }
    if (sscanf(data, "!OSEL=%d", &value) == 1) {
        configStatus["OSEL"] = true;
        if (value != 0) {
            configStatus["OSEL"] = false;
            WLOG(("%s: SerialA2D OSEL not default: val=%d",
                getName().c_str(), value ));
        }
        return;
    }
    if (sscanf(data, "!FILT=%d", &value) == 1) {
        configStatus["FILT"] = true;
        if (value == 10)
            _staticLag *= 5;   // TempDACQ shifts 5 samples.
        else if ((value == 0 || value == 5) && _sampleRate != 250)
            configStatus["FILT"] = false;
        else if ((value == 1 || value == 6 || value == 10) && _sampleRate != 100)
            configStatus["FILT"] = false;
        else if ((value == 2 || value == 7) && _sampleRate != 50)
            configStatus["FILT"] = false;
        else if ((value == 3 || value == 8) && _sampleRate != 25)
            configStatus["FILT"] = false;
        else if ((value == 4 || value == 9) && _sampleRate != 10)
            configStatus["FILT"] = false;
        return;
    }
}

void A2D_Serial::extractStatus(const char *msg, int len)
{
    // msg is terminated with a NULL char
    const char *p = msg;
    int cnt = 0;

    for (int i = 0; i < len && cnt < 2; ++i)
        if (*p++ == ',')
            ++cnt;
    if (cnt == 2 && p-msg < len)
        configStatus["PPS"] = atoi(p);
}


void A2D_Serial::readCalFile(dsm_time_t tt) throw()
{
    if (!_calFile) return;

    if (getOutputMode() == Counts)
        return;

    // Read CalFile  containing the following fields after the time
    // gain polarity(ignored for this card) intcp0 slope0 intcp1 slope1 ... intcp7 slope7

    while (tt >= _calFile->nextTime().toUsecs()) {
        int nd = 2 + getMaxNumChannels() * 4;
        float d[nd];
        try {
            n_u::UTime calTime;
            int n = _calFile->readCF(calTime, d,nd);
            if (n < 2) continue;
            int cgain = (int)d[0];
            for (int i = 0;
                i < std::min((n-2)/4,getMaxNumChannels()); i++) {
                    int gain = getGain(i);
                    if (cgain < 0 || gain == cgain)
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
    int n)
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
    return _bipolar[ichan];
}

bool A2D_Serial::samplingChannel(int channel) const
{
    return channel >= 0 && channel < getMaxNumChannels() && _ivarByChan[channel] >= 0;
}

float A2D_Serial::applyCalibration(float value, const std::vector<float> &cals) const
{
    float out = value;
    if (cals.size() > 0)
    {
        int corder = cals.size() - 1;
        out = cals[corder];
        for (size_t k = 1; k < cals.size(); k++)
          out = cals[corder-k] + value * out;

    }
    return out;
}

void A2D_Serial::executeXmlRpc(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw()
{
    string action = "null";
    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
        action = string(params["action"]);
    }
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray) {
        action = string(params[0]["action"]);
    }

    if      (action == "testVoltage") testVoltage(params,result);
    else if (action == "getA2DSetup") getA2DSetup(params,result);
    else {
        string errmsg = "XmlRpc error: " + getName() + ": no such action " + action;
        PLOG(("Error: ") << errmsg);
        result = errmsg;
        return;
    }
}

void A2D_Serial::getA2DSetup(XmlRpc::XmlRpcValue&, XmlRpc::XmlRpcValue& result)
        throw()
{
    result["card"] = "gpDAQ";
    result["nChannels"] = getMaxNumChannels();
    for (int i = 0; i < getMaxNumChannels(); i++) {
        result["gain"][i]   = _gains[i];
        result["offset"][i] = _bipolar[i] ? 0 : 1;
        result["calset"][i] = (_calset & (1 << i)) ? 1 : 0;
    }
    result["vcal"]      = _voltage;
    DLOG(("%s: result:",getName().c_str()) << result.toXml());
}

void A2D_Serial::testVoltage(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw()
{
    int state   = 0;    // 0 = turn off, 1 = turn on
    _calset  = 0;       // each bit represents that channel
    _voltage = 0;

    string errmsg = "XmlRpc error: testVoltage: " + getName();

    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
        _voltage = params["voltage"];
        _calset  = params["calset"];
        state   = params["state"];
    }
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray) {
        _voltage = params[0]["voltage"];
        _calset  = params[0]["calset"];
        state   = params[0]["state"];
    }
    if (_calset < 0 || 0xff < _calset) {
        char hexstr[50];
        sprintf(hexstr, "0x%x", _calset);
        errmsg += ": invalid calset: " + string(hexstr);
        PLOG(("") << errmsg);
        result = errmsg;
        return;
    }

    if (state == 0) {
        _calset = 0;
        _voltage = -99;
    }

    // set the test voltage and channel(s)
    try {
        write("#RST\n", 5);        // reset device to turn off existing
        if (state == 1 && _voltage >= 0) {
            char cmd_str[32];
            int cmd = 3;    // default to 2.5 vdc.
            if (_voltage == 0)
                cmd = 2;    // zero vdc
            for (int i = 0; i < getMaxNumChannels(); i++) {
                if ((_calset>>i) & 0x0001) {
                    sprintf(cmd_str, "#ISEL,%d,%d\n", i, cmd);
                    write(cmd_str, strlen(cmd_str));
                }
            }
        }
    }
    catch(const n_u::IOException& e) {
        string errmsg = "XmlRpc error: testVoltage: " + getName() + ": " + e.what();
        PLOG(("") << errmsg);
        result = errmsg;
        return;
    }
    result = "success";
    DLOG(("%s: result:",getName().c_str()) << result.toXml());
}
