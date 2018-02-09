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

#include "SppSerial.h"
#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/util/UTime.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

//NIDAS_CREATOR_FUNCTION_NS(raf,SppSerial)

SppSerial::~SppSerial()
{
    delete [] _waitingData;
    if (_totalRecordCount > 0) {
        cerr << "SppSerial::" << _probeName << ": " << _skippedRecordCount <<
            " records skipped of " << _totalRecordCount << " records for " <<
            ((float)_skippedRecordCount/_totalRecordCount) * 100 << "% loss.\n";
    }
}

SppSerial::SppSerial(const std::string & probe) : DSMSerialSensor(),
    _model(0),
    _nChannels(0),_nHskp(0),
    _probeName(probe),
    _range(0),_triggerThreshold(0),
    _opcThreshold(),_noutValues(0),
    _dataType(FixedLength),_recDelimiter(0),
    _checkSumErrorCnt(0),
    _waitingData(0),
    _nWaitingData(0),
    _skippedBytes(0),
    _skippedRecordCount(0),
    _totalRecordCount(0),
    // _sampleRate(1),
    _outputDeltaT(false),
    _prevTime(0),
    _converters()
{
    // If these aren't true, we're screwed!
    assert(sizeof(DMT_UShort) == 2);
    assert(sizeof(DMT_ULong) == 4);
}

unsigned short SppSerial::computeCheckSum(const unsigned char * pkt, int len)
{
    unsigned short sum = 0;
    // Compute the checksum of a series of chars
    // Sum the byte count and data bytes;
    for (int j = 0; j < len; j++) 
        sum += (unsigned short)pkt[j];
    return sum;
}

void SppSerial::validate() throw(n_u::InvalidParameterException)
{
    // _sampleRate = (int)rint(getPromptRate());

    if (_probeName.compare("PIP"))  // If not the PIP probe.
    {
        const Parameter *p;

        /* Parameters common to CDP_Serial, SPP*00_Serial */
        p = getParameter("NCHANNELS");
        if (!p) throw n_u::InvalidParameterException(getName(),
          "NCHANNELS", "not found");
        _nChannels = (int)p->getNumericValue(0);

        p = getParameter("RANGE");
        if (!p) throw n_u::InvalidParameterException(getName(),
          "RANGE", "not found");
        _range = (unsigned short)p->getNumericValue(0);

        p = getParameter("THRESHOLD");
        if (!p) throw n_u::InvalidParameterException(getName(),
          "THRESHOLD","not found");
        _triggerThreshold = (unsigned short)p->getNumericValue(0);

        p = getParameter("CHAN_THRESH");
        if (!p) 
            throw n_u::InvalidParameterException(getName(), "CHAN_THRESH", "not found");
        if (p->getLength() != _nChannels)
            throw n_u::InvalidParameterException(getName(), "CHAN_THRESH", 
                    "not NCHANNELS long ");
        for (int i = 0; i < p->getLength(); ++i)
            _opcThreshold[i] = (unsigned short)p->getNumericValue(i);
    }

    /* Check requested variables */
    list<SampleTag*>& tags = getSampleTags();
    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName(), "sample",
                "must be one and only one <sample> tag for this sensor");

    _noutValues = 0;
    list<SampleTag*>::const_iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti)
    {
        SampleTag* stag = *ti;

        const vector<Variable*>& vars = stag->getVariables();
        vector<Variable*>::const_iterator vi = vars.begin();
        for ( ; vi != vars.end(); ++vi)
        {
            Variable* var = *vi;
            _noutValues += var->getLength();
            if (getApplyVariableConversions())
                _converters.push_back(var->getConverter());
            if (var->getName().compare(0, 6, "DELTAT") == 0) {
                _outputDeltaT = true;
            }
        }
    }

#ifdef ZERO_BIN_HACK
    /*
     * We'll be adding a bogus zeroth bin to the data to match historical 
     * behavior. Remove all traces of this after the netCDF file refactor.
     */
    if (_noutValues != _nChannels + _nHskp + (int) _outputDeltaT +1) {
        ostringstream ost;
        ost << "total length of variables should be " << 
            (_nChannels + _nHskp + (int)_outputDeltaT + 1) << " rather than " << _noutValues << ".\n";
        throw n_u::InvalidParameterException(getName(), "sample",
                ost.str());
    }
#else
    if (_noutValues != _nChannels + _nHskp + (int) _outputDeltaT) {
        ostringstream ost;
        ost << "total length of variables should be " << 
            (_nChannels + _nHskp + (int) _outputDeltaT) << " rather than " << _noutValues << ".\n";
        throw n_u::InvalidParameterException(getName(), "sample",
                ost.str());
    }
#endif

    /*
     * Allocate a new buffer for yet-to-be-handled data.  We get enough space
     * to hold up to two full samples.
     */
    delete[] _waitingData;

    _waitingData = new unsigned char[2 * packetLen()];
    _nWaitingData = 0;

    DSMSerialSensor::validate();
}

void SppSerial::sendInitPacketAndCheckAck(void * setup_pkt, int len, int return_len)
throw(n_u::IOException)
{   
    std::string eType("SppSerial init-ack");

    try {
        setMessageParameters(1,"",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"send init",e.what());
    }

    // clear whatever junk may be in the buffer til a timeout
    try {
        for (;;) {
            readBuffer(MSECS_PER_SEC / 100);
            clearBuffer();
        }
    }
    catch (const n_u::IOTimeoutException& e) {}

    // The initialization response is two bytes 0x0606
    try {
        setMessageParameters(return_len, "", true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"send init",e.what());
    }

    n_u::UTime twrite;

    ILOG(("%s: sending packet, length=%d",getName().c_str(),len));
    write(setup_pkt, len);

    //
    // Get the response
    //

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    unsigned short response;
    char* dp = (char*) &response;
    for (int lres = 0; lres < 2; ) {
        size_t l = read(dp,sizeof(response)-lres,MSECS_PER_SEC * 5);
        lres += l;
        dp += l;
    }

    // 
    // see if we got the expected response
    //
    if (response != 0x0606)
    {
        ostringstream ost;
        ost << hex << showbase << internal << setfill('0') << setw(4) << response;
        throw n_u::IOException(getName(), eType, string("received ") + ost.str() + " instead of the expected return of 0x0606.");
    }
    else {
        n_u::UTime tread;
        ILOG(("%s: received init ack after ",getName().c_str()) <<
                (float)(tread.toUsecs() - twrite.toUsecs()) / USECS_PER_SEC << " seconds");
    }
}

int SppSerial::appendDataAndFindGood(const Sample* samp)
{
    if ((signed)samp->getDataByteLength() != packetLen()) 
        return false;

    /*
     * Add the sample to our waiting data buffer
     */
    assert(_nWaitingData <= packetLen());
    ::memcpy(_waitingData + _nWaitingData, samp->getConstVoidDataPtr(), 
            packetLen());
    _nWaitingData += packetLen();

    /*
     * Hunt in the waiting data until we find a packetLen() sized stretch
     * where the last two bytes are a good checksum for the rest or match
     * the expected record delimiter.  Most of the time, we should find it 
     * on the first pass through the loop.
     */
    bool foundRecord = 0;
    for (int offset = 0; offset <= (_nWaitingData - packetLen()); offset++) {
        unsigned char *input = _waitingData + offset;
        DMT_UShort packetCheckSum;
        ::memcpy(&packetCheckSum, input + packetLen() - 2, 2);
        switch (_dataType) 
        {
        case Delimited:
            foundRecord = (UnpackDMT_UShort(packetCheckSum) == _recDelimiter);
            break;
        case FixedLength:
            foundRecord = (computeCheckSum(input, packetLen() - 2) == 
                    UnpackDMT_UShort(packetCheckSum));
            break;
        }

        if (foundRecord)
        {
            /*
             * Drop data so that the good record is at the beginning of
             * _waitingData
             */
            if (offset > 0)	{
                _nWaitingData -= offset;
                ::memmove(_waitingData, _waitingData + offset, _nWaitingData);

                _skippedBytes += offset;
            }

            if (_skippedBytes) {
                //	  cerr << "SppSerial::appendDataAndFind(" << _probeName << ") skipped " <<
                //		_skippedBytes << " bytes to find a good " << packetLen() << "-byte record.\n";
                _skippedBytes = 0;
                _skippedRecordCount++;
            }

            _totalRecordCount++;
            return true;
        }
    }

    /*
     * If we didn't find a good record, keep the last packetLen()-1 bytes of
     * the waiting data and wait for the next blob of input.
     */
    int nDrop = _nWaitingData - (packetLen() - 1);
    _nWaitingData -= nDrop;
    ::memmove(_waitingData, _waitingData + nDrop, _nWaitingData);

    _skippedBytes += nDrop;

    return false;
}

