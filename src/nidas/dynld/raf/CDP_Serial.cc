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


CDP_Serial::CDP_Serial(): SppSerial()
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
    _nHskp = 8;
}


void CDP_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SppSerial::fromDOMElement(node);

    // If fixed record delimiter.
    //
    if (getMessageSeparator().length() > 0) {	// PACDEX
        _dataType = Delimited;
        _recDelimiter = 0xffff;
    }

    const Parameter *p;

    p = getParameter("THRESHOLD");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "THRESHOLD","not found");
    _triggerThreshold = (unsigned short)p->getNumericValue(0);

    p = getParameter("TRANSIT_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "TRANSIT_REJ","not found");
    _transitReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DOF_REJ","not found");
    _dofReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("ATT_ACCEPT");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "ATT_ACCEPT","not found");
    _attAccept = (unsigned short)p->getNumericValue(0);

    p = getParameter("DIVISOR_FLAG");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DIVISOR_FLAG","not found");
    _divFlag = (unsigned short)p->getNumericValue(0);

    p = getParameter("CT_METHOD");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "CT_METHOD","not found");
    _ctMethod = (unsigned short)p->getNumericValue(0);
}

void CDP_Serial::sendInitString() throw(n_u::IOException)
{
    InitCDP_blk setup_pkt;

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    PackDMT_UShort(setup_pkt.trig_thresh, _triggerThreshold);
    PackDMT_UShort(setup_pkt.transRej, _transitReject);
    PackDMT_UShort(setup_pkt.chanCnt, (unsigned short)_nChannels);
    PackDMT_UShort(setup_pkt.dofRej, _dofReject);
    PackDMT_UShort(setup_pkt.range, _range);
    PackDMT_UShort(setup_pkt.avTranWe, _avgTransitWeight);
    PackDMT_UShort(setup_pkt.attAccept, _attAccept);
    PackDMT_UShort(setup_pkt.divFlag, _divFlag);
    PackDMT_UShort(setup_pkt.ct_method, _ctMethod);

    for (int i = 0; i < _nChannels; i++)
	PackDMT_UShort(setup_pkt.OPCthreshold[i], _opcThreshold[i]);

    // exclude chksum from the computation
    PackDMT_UShort(setup_pkt.chksum,
		   computeCheckSum((unsigned char*)&setup_pkt,
				   _InitPacketSize - 2));
    sendInitPacketAndCheckAck(&setup_pkt, _InitPacketSize);

    setMessageLength(packetLen());
    setMessageParameters();
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

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float * dout = outs->getDataPtr();
    float value;
    const float * dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = UnpackDMT_UShort(inRec.cabinChan[FLSR_CUR_INDX]) * (76.3 / 1250);
    *dout++ = UnpackDMT_UShort(inRec.cabinChan[FLSR_PWR_INDX]) * (0.5 / 408);

    value = UnpackDMT_UShort(inRec.cabinChan[FWB_TMP_INDX]);
    *dout++ = (1.0 / ((1.0 / 3750.0) * log((4096.0 / value) - 1.0) + (1.0 / 298.0))) - 273.0;

    value = UnpackDMT_UShort(inRec.cabinChan[FLSR_TMP_INDX]);
    *dout++ = (1.0 / ((1.0 / 3900.0) * log((4096.0 / value) - 1.0) + (1.0 / 298.0))) - 273.0;

    *dout++ = UnpackDMT_ULong(inRec.rejDOF);
    *dout++ = UnpackDMT_ULong(inRec.rejAvgTrans);
    *dout++ = UnpackDMT_ULong(inRec.AvgTransit);
    *dout++ = UnpackDMT_ULong(inRec.ADCoverflow);

#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif
    for (int iout = 0; iout < _nChannels; ++iout)
	*dout++ = UnpackDMT_ULong(inRec.OPCchan[iout]) * _sampleRate;

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
