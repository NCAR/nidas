// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3654 $

    $LastChangedDate: 2007-02-01 14:40:14 -0700 (Thu, 01 Feb 2007) $

    $LastChangedRevision: 3654 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/CDP_Serial.cc $

*/

#include <nidas/dynld/raf/CDP_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CDP_Serial)

const size_t CDP_Serial::FLSR_CUR_INDX = 0;
const size_t CDP_Serial::FLSR_PWR_INDX = 1;
const size_t CDP_Serial::FWB_TMP_INDX = 2;
const size_t CDP_Serial::FLSR_TMP_INDX = 3;
const size_t CDP_Serial::SIZER_BLINE_INDX = 4;
const size_t CDP_Serial::QUAL_BLINE_INDX = 5;
const size_t CDP_Serial::VDC5_MON_INDX = 6;
const size_t CDP_Serial::FCB_TMP_INDX = 7;


CDP_Serial::CDP_Serial(): SppSerial("CDP"),
    _dofReject(0)
{
    //
    // Make sure we got compiled with the packet structs packed appropriately.
    // If any of these assertions fails, then we can't just memcpy() between
    // the actual DMT packets and these structs, and we count on being able to
    // do that...
    //
    char* headPtr;
    char* chksumPtr;
    
    InitCDP_blk init;
    headPtr = (char*)&init;
    chksumPtr = (char*)&(init.chksum);
    assert((chksumPtr - headPtr) == (_InitPacketSize - 2));
    
    _nChannels = MAX_CHANNELS; // use a packet length containing all channels
    CDP_blk data;
    headPtr = (char*)&data;
    chksumPtr = (char*)&(data.chksum);
    assert((chksumPtr - headPtr) == (packetLen() - 2));
    _nChannels = 0; // back to zero until it gets set via configuration
    
    //
    // Model number is fixed
    //
    _model = 100;

    //
    // This number should match the housekeeping added in ::process, so that
    // an output sample of the correct size is created.
    //
    _nHskp = 15;

}

void CDP_Serial::validate()
    throw(n_u::InvalidParameterException)
{
    // If fixed record delimiter.
    if (getMessageSeparator().length() > 0) {	// PACDEX
        _dataType = Delimited;
        _recDelimiter = 0xffff;
    }

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

void CDP_Serial::sendInitString() throw(n_u::IOException)
{
    // zero initialize
    InitCDP_blk setup_pkt = InitCDP_blk();

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    PackDMT_UShort(setup_pkt.trig_thresh, _triggerThreshold);
    PackDMT_UShort(setup_pkt.transRej, 0);
    PackDMT_UShort(setup_pkt.chanCnt, (unsigned short)_nChannels);
    PackDMT_UShort(setup_pkt.dofRej, _dofReject);
    PackDMT_UShort(setup_pkt.range, _range);
    PackDMT_UShort(setup_pkt.avTranWe, 0);
    PackDMT_UShort(setup_pkt.attAccept, 0);
    PackDMT_UShort(setup_pkt.divFlag, 0);
    PackDMT_UShort(setup_pkt.ct_method, 0);

    for (int i = 0; i < _nChannels; i++)
	PackDMT_UShort(setup_pkt.OPCthreshold[i], _opcThreshold[i]);

    // exclude chksum from the computation
    PackDMT_UShort(setup_pkt.chksum,
		   computeCheckSum((unsigned char*)&setup_pkt,
				   _InitPacketSize - 2));

    // Expect 4 byte ack from CDP, instead of normal 2 for other probes.
    sendInitPacketAndCheckAck(&setup_pkt, _InitPacketSize, 4);

    try {
        setMessageParameters(packetLen(),"",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"init",e.what());
    }
}

bool CDP_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if (! appendDataAndFindGood(samp))
        return false;

    /*
     * Copy the good record into our CDP_blk struct.
     */
    CDP_blk inRec;

    ::memcpy(&inRec, _waitingData, packetLen() - 2);
    ::memcpy(&inRec.chksum, _waitingData + packetLen() - 2, 2);

    /*
     * Shift the remaining data in _waitingData to the head of the line
     */
    _nWaitingData -= packetLen();
    ::memmove(_waitingData, _waitingData + packetLen(), _nWaitingData);

    /*
     * Create the output stuff
     */
    SampleT<float>* outs = getSample<float>(_noutValues);

    dsm_time_t ttag = samp->getTimeTag();
    outs->setTimeTag(ttag);
    outs->setId(getId() + 1);

    float * dout = outs->getDataPtr();
    float value;
    const float * dend = dout + _noutValues;
    unsigned int ivar = 0;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[FLSR_CUR_INDX]) * (76.3 / 1250),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[FLSR_PWR_INDX]) * (0.5 / 408),ivar++);

    value = UnpackDMT_UShort(inRec.cabinChan[FWB_TMP_INDX]);
    *dout++ = convert(ttag,(1.0 / ((1.0 / 3750.0) * log((4096.0 / value) - 1.0) + (1.0 / 298.0))) - 273.0,ivar++);

    value = UnpackDMT_UShort(inRec.cabinChan[FLSR_TMP_INDX]);
    *dout++ = convert(ttag,(1.0 / ((1.0 / 3900.0) * log((4096.0 / value) - 1.0) + (1.0 / 298.0))) - 273.0,ivar++);

    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[SIZER_BLINE_INDX]) * (0.5 / 408),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[QUAL_BLINE_INDX]) * (0.5 / 408),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[VDC5_MON_INDX]) * (0.5 / 408),ivar++);
    value = UnpackDMT_UShort(inRec.cabinChan[FCB_TMP_INDX]);
    *dout++ = convert(ttag,0.06401 * value - 50.0,ivar++);

    *dout++ = convert(ttag,UnpackDMT_ULong(inRec.rejDOF),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.QualBndwdth),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.QualThrshld),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.AvgTransit) * 0.025,ivar++);   // 40MHz clock.
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.SizerBndwdth),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.SizerThrshld),ivar++);
    *dout++ = convert(ttag,UnpackDMT_ULong(inRec.ADCoverflow),ivar++);

#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif
    for (int iout = 0; iout < _nChannels; ++iout)
	*dout++ = UnpackDMT_ULong(inRec.OPCchan[iout]);

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
