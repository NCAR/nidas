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

#include "PIP_Serial.h"
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>
#include <sys/time.h>
using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PIP_Serial)

const size_t PIP_Serial::PIPEDV0 = 0;
const size_t PIP_Serial::PIPEDV64= 1;
const size_t PIP_Serial::PIPEDV32= 2;
const size_t PIP_Serial::PIPQC= 3;
const size_t PIP_Serial::PIPPS= 4;
const size_t PIP_Serial::PIPLWC = 5;
const size_t PIP_Serial::PIPLWCSLV= 6;
const size_t PIP_Serial::PIPCBTMP = 7;
const size_t PIP_Serial::PIPRH = 8;
//skipping the optional input 1-4, which are currently undefined
const size_t PIP_Serial::PIPRT = 13;
const size_t PIP_Serial::PIPLSRC = 14;
const size_t PIP_Serial::PIPLSRP = 15;


PIP_Serial::PIP_Serial(): SppSerial("PIP"),
    _dofReject(1), _airspeedSource(1), _trueAirSpeed(floatNAN)
{
    //
    // Make sure we got compiled with the packet structs packed appropriately.
    // If any of these assertions fails, then we can't just memcpy() between
    // the actual DMT packets and these structs, and we count on being able to
    // do that...
    //
    char* headPtr;
    char* chksumPtr;

    InitPIP_blk init;
    headPtr = (char*)&init;
    chksumPtr = (char*)&(init.chksum);
    assert((chksumPtr - headPtr) == (_InitPacketSize - 2));

    _nChannels = N_PIP_CHANNELS;
    PIP_blk data;
    headPtr = (char*)&data;
    chksumPtr = (char*)&(data.chksum);
    assert((chksumPtr - headPtr) == (packetLen() - 4));

    //
    // This number should match the housekeeping added in ::process, so that
    // an output sample of the correct size is created.
    //
    _nHskp = 9;

}

/*---------------------------------------------------------------------------*/
void PIP_Serial::validate()
    throw(n_u::InvalidParameterException)
{
    // Need this if fixed record delimiter, to ensure
    // it doesn't try to check the checksum at packetLen()-2
    // seeing as this has the checksum at packetLen()-4 due to the trailers.
//    if (getMessageSeparator().length() > 0) {
//        _dataType = Delimited;
//    }

    const Parameter *p;

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DOF_REJ","not found");
    _dofReject = (unsigned short)p->getNumericValue(0);

    // Initialize the message parameters to something that passes
    // SppSerial::validate(). The packet length
    // is not actually yet known, because it depends on _nChannels
    // which is set in SppSerial::validate(). This prevents an
    // InvalidParameterException in SppSerial::validate(),
    // until we can set it later.
    try {
        setMessageParameters(packetLen(),"",true);
    }
    catch(const n_u::IOException& e) {
        throw n_u::InvalidParameterException(getName(),"message parameters",e.what());
    }

    SppSerial::validate();
}

void PIP_Serial::sendTimePacket() throw(n_u::IOException)
{
    //send time to probe.
    SetAbsoluteTime setTime_pkt = SetAbsoluteTime();
    setTime_pkt.esc = 0x1b;
    setTime_pkt.id = 0x05;

    //is recomended that gettimeofday be replaced with clock_gettime for better accuracy
    //however, clock_gettime doesn't exist for our version of armbe so keep it
    //gettimeofday to not break the build.
    struct timeval tv;
    if( gettimeofday( &tv,NULL) != -1 ) {
        //extranious chars here are to deal with overflows
        char h = (char)tv.tv_sec/3600;
        setTime_pkt.hour=h;
        char m = tv.tv_sec/60;
        setTime_pkt.hour=m;
        char s = tv.tv_sec;
        setTime_pkt.hour=s;
        char mili = (char)tv.tv_usec/1000;
        setTime_pkt.hour=mili;
    }
     //time packet should have a size of 8, including checksum
     PackDMT_UShort(setTime_pkt.chksum,
                   computeCheckSum((unsigned char*)&setTime_pkt,
                                   _setTimePacketSize - 2));
  sendInitPacketAndCheckAck(&setTime_pkt, _setTimePacketSize ,4);

}

/*---------------------------------------------------------------------------*/
void PIP_Serial::sendInitString() throw(n_u::IOException)
{
    // zero initialize
    InitPIP_blk setup_pkt = InitPIP_blk();

    //init packet setup
    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    PackDMT_UShort(setup_pkt.airspeedSource, _airspeedSource); //0x0001: host = computer which sends packet
    PackDMT_UShort(setup_pkt.dofRej, _dofReject);
    setup_pkt.pSizeDim = 0x01;
    setup_pkt.rc = 0xFF;

    // exclude chksum from the computation
    PackDMT_UShort(setup_pkt.chksum,
		   computeCheckSum((unsigned char*)&setup_pkt,
				   _InitPacketSize - 2));

    // Expect 4 byte ack from PIP, instead of normal 2 for other probes.
    sendInitPacketAndCheckAck(&setup_pkt, _InitPacketSize, 4);
//cerr<<"init, before time"<<endl;

//    sendTimePacket();

//cerr<<"init, after time"<<endl;

    //may need lock on xml or something to prevent half formed pas
    //can I just modify the 4 relevant bytes?

    try {
        setMessageParameters(packetLen(),"",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"init",e.what());
    }
}

bool PIP_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if (! appendDataAndFindGood(samp))
        return false;


    // Copy the good record into our PIP_blk struct.
    PIP_blk inRec;

    ::memcpy(&inRec, _waitingData, packetLen() - 2);
    ::memcpy(&inRec.chksum, _waitingData + packetLen() - 4, 2);

    // Shift the remaining data in _waitingData to the head of the line
    _nWaitingData -= packetLen();
    ::memmove(_waitingData, _waitingData + packetLen(), _nWaitingData);

//    cerr<<"reset flag:"<<UnpackDMT_UShort(inRec.resetFlag)<<endl;
//    cerr<<"time hour:min "<<inRec.hour<<":"<<inRec.min<<endl;
//    cerr<<"time :"<<UnpackDMT_UShort(inRec.SecMili)<<endl;

    // Create the output stuff
    SampleT<float>* outs = getSample<float>(_noutValues);

    dsm_time_t ttag = samp->getTimeTag();
    outs->setTimeTag(ttag);
    outs->setId(getId() + 1);

    float * dout = outs->getDataPtr();
    const float * dend = dout + _noutValues;
    unsigned int ivar = 0;

    *dout++ = convert(ttag,20.0*(UnpackDMT_UShort(inRec.housekeeping[PIPEDV0])/4095.0), ivar++);
    *dout++ = convert(ttag,20.0*(UnpackDMT_UShort(inRec.housekeeping[PIPEDV64])/4095.0), ivar++);
    *dout++ = convert(ttag,20.0*(UnpackDMT_UShort(inRec.housekeeping[PIPEDV32])/4095.0), ivar++);

//    *dout++ = convert(ttag,70.786*5*(UnpackDMT_UShort(inRec.housekeeping[PIPQC])/4095.0), ivar++);
//    *dout++ = convert(ttag,206.88*5*(UnpackDMT_UShort(inRec.housekeeping[PIPPS])/4095.0), ivar++);
//    *dout++ = convert(ttag,10.0*(UnpackDMT_UShort(inRec.housekeeping[PIPLWC])/4095.0), ivar++);
//    *dout++ = convert(ttag,10.0*(UnpackDMT_UShort(inRec.housekeeping[PIPLWCSLV])/4095.0), ivar++);

    *dout++ = convert(ttag,(UnpackDMT_UShort(inRec.housekeeping[PIPCBTMP])-599.0)*20.0/4095.0, ivar++);

 //   *dout++ = convert(ttag,32.258*10*(UnpackDMT_UShort(inRec.housekeeping[PIPRH])/4095.0)-25.81, ivar++);
 //   *dout++ = convert(ttag,((UnpackDMT_UShort(inRec.housekeeping[PIPRT])*100.0/4095.0)-50), ivar++);
    *dout++ = convert(ttag,(UnpackDMT_UShort(inRec.housekeeping[PIPLSRC])*.122), ivar++);
    *dout++ = convert(ttag,(UnpackDMT_UShort(inRec.housekeeping[PIPLSRP])*10.0/4095.0), ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.oversizeReject), ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.DOFRejectCount), ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.EndRejectCount), ivar++);


#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif
    for (int iout = 0; iout < _nChannels; ++iout)
	*dout++ = UnpackDMT_UShort(inRec.binCount[iout]);

    // Compute DELTAT.
    if (_outputDeltaT) {
        if (_prevTime != 0)
            *dout++ = (ttag - _prevTime) / USECS_PER_SEC;
        else *dout++ = 0.0;
        _prevTime = ttag;
    }

    // If this fails then the correct pre-checks weren't done in validate().
    assert(dout == dend);

    results.push_back(outs);

    return true;
}

/*---------------------------------------------------------------------------*/
int PIP_Serial::appendDataAndFindGood(const Sample* samp)
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
        ::memcpy(&packetCheckSum, input + packetLen() - 4, 2); //compensate for trailer
        switch (_dataType)
        {
        case Delimited:
            foundRecord = (UnpackDMT_UShort(packetCheckSum) == _recDelimiter);
            break;
        case FixedLength:
            foundRecord = (computeCheckSum(input + 2, packetLen() - 6) == //compensate for trailer and header
                    UnpackDMT_UShort(packetCheckSum));
            break;
        }

        if (foundRecord)
        {
            /*
             * Drop data so that the good record is at the beginning of
             * _waitingData
             */
            if (offset > 0)     {
                _nWaitingData -= offset;
                ::memmove(_waitingData, _waitingData + offset, _nWaitingData);

                _skippedBytes += offset;
            }

            if (_skippedBytes) {
//                cerr << "SppSerial::appendDataAndFind(" << _probeName << ") skipped " <<
//                _skippedBytes << " bytes to find a good " << packetLen() << "-byte record.\n";
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

/*---------------------------------------------------------------------------*/

void PIP_Serial::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
cerr<<"PIP_Serial Open"<<endl;
    DSMSerialSensor::open(flags);
cerr<<"DSMSErial:: open done "<<endl;
//    init_parameters();

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                getName().c_str(),
                "no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}

void PIP_Serial::close() throw(n_u::IOException)
{
    if (DerivedDataReader::getInstance())
            DerivedDataReader::getInstance()->removeClient(this);
    DSMSensor::close();
}

/*---------------------------------------------------------------------------*/

void PIP_Serial::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
    SendPIP_BLK send_data_pkt;
    send_data_pkt.esc = 0x1b;
    send_data_pkt.id = 0x02;
    PackDMT_UShort(send_data_pkt.hostSyncCounter , 0x0001); //use packDMT_Ushort here, right below and checksum
    PackDMT_UShort(send_data_pkt.relayControl,  0x0000);

    _trueAirSpeed = s->getTrueAirspeed();   // save it to display in printStatus
//cerr<<"trueairspeed:"<<_trueAirSpeed<<endl;
//
    //calculate tas - get resolution out of validate
    unsigned long n = (unsigned long) (_trueAirSpeed / (10e-4) * 34.415);

    PackDMT_ULong(send_data_pkt.PASCoefficient, n);
    PackDMT_UShort(send_data_pkt.chksum , computeCheckSum((unsigned char*)&send_data_pkt, 10));
    string temp;
    temp.resize(13);
    ::memcpy(&temp[0], (const void *)&send_data_pkt , 12);
    temp[12] = 0;

    try {
        setPrompt(Prompt(temp));
    }
    catch(const n_u::IOException & e)
    {
        n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
    }
}
