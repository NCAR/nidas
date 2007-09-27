/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/raf/SPP100_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, SPP100_Serial)

const size_t SPP100_Serial::FREF_INDX = 4;
const size_t SPP100_Serial::FTMP_INDX = 7;


SPP100_Serial::SPP100_Serial(): SppSerial()
{
  _model = 100;
  // This number should match the housekeeping added in ::process, so that
  // an output sample of the correct size is created.
  _nHskp = 6;
}


void SPP100_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SppSerial::fromDOMElement(node);

    const Parameter *p;

    p = getParameter("THRESHOLD");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "THRESHOLD", "not found");
    _triggerThreshold = (unsigned short)p->getNumericValue(0);

    p = getParameter("TRANSIT_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "TRANSIT_REJ", "not found");
    _transitReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "DOF_REJ", "not found");
    _dofReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("ATT_ACCEPT");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "ATT_ACCEPT", "not found");
    _attAccept = (unsigned short)p->getNumericValue(0);

    p = getParameter("DIVISOR_FLAG");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "DIVISOR_FLAG", "not found");
    _divFlag = (unsigned short)p->getNumericValue(0);

    p = getParameter("CT_METHOD");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "CT_METHOD", "not found");
    _ctMethod = (unsigned short)p->getNumericValue(0);

    p = getParameter("MAX_WIDTH");
    if (!p) throw n_u::InvalidParameterException(getName(), 
          "MAX_WIDTH", "not found");
    _maxWidth = (unsigned short)p->getNumericValue(0);
}

void SPP100_Serial::sendInitString() throw(n_u::IOException)
{
    Init100_blk setup_pkt;

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    setup_pkt.model.putValue(_model);
    setup_pkt.trig_thresh.putValue(_triggerThreshold);
    setup_pkt.transRej.putValue(_transitReject);
    setup_pkt.chanCnt.putValue(_nChannels);
    setup_pkt.dofRej.putValue(_dofReject);
    setup_pkt.range.putValue(_range);
    setup_pkt.avTranWe.putValue(_avgTransitWeight);
    setup_pkt.attAccept.putValue(_attAccept);
    setup_pkt.divFlag.putValue(_divFlag);
    setup_pkt.ct_method.putValue(_ctMethod);
    setup_pkt.max_width.putValue(_maxWidth);

    for (int i = 0; i < _nChannels; i++)
        setup_pkt.OPCthreshold[i].putValue(_opcThreshold[i]);

    // exclude chksum from the computation (but since it is zero
    // at this point, it doesn't really matter).
    setup_pkt.chksum.putValue(computeCheckSum((unsigned char*)&setup_pkt, 
					      sizeof(setup_pkt) - 2));

    setMessageLength(1);
    setMessageSeparator("");
    setMessageParameters();

    // clear whatever junk may be in the buffer til a timeout
    try {
        for (;;) {
            readBuffer(MSECS_PER_SEC / 100);
            clearBuffer();
        }
    }
    catch (const n_u::IOTimeoutException& e) {}

    setMessageLength(sizeof(Response100_blk));
    setMessageParameters();

    write(&setup_pkt, sizeof(setup_pkt));

    // Build the expected response, which looks a lot like the init 
    // packet.  Because of their similarity, we memcpy large chunks between 
    // them for simplicity.
    Response100_blk expected_return;

    // directly copy all fields from "esc" to "model" (4 bytes), stuff
    // in the expected firmware value, then directly copy the fields from
    // "trig_thresh" to "OPCthreshold" (100 bytes)
    ::memcpy(&expected_return.esc, &setup_pkt.esc, 4);
    expected_return.firmware.putValue(0x105);
    ::memcpy(&expected_return.trig_thresh, &setup_pkt.trig_thresh, 100);

    // calculate the expected checksum and stuff that in, too
    unsigned short checkSum = computeCheckSum((unsigned char*)&expected_return,
					      sizeof(expected_return) - 2);
    expected_return.chksum.putValue(checkSum);


    //
    // Get the response
    //

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    readBuffer(MSECS_PER_SEC * 3);

    Sample* samp = nextSample();
    if (!samp) 
        throw n_u::IOException(getName(), 
            "S100 init return packet", "not read");


    if (samp->getDataByteLength() != sizeof(Response100_blk)) {
        ostringstream ost;
        ost << "S100 init return packet, wrong size=" <<
            samp->getDataByteLength() <<
            " expected=" << sizeof(Response100_blk) << endl;
        samp->freeReference();
        throw n_u::IOException(getName(), "sendInitString", ost.str());
    }

    // pointer to the returned data
    Response100_blk* init_return = (Response100_blk*) samp->getVoidDataPtr();

    //
    // See if the response matches what we expect
    //
    if (::memcmp(init_return, &expected_return, sizeof(init_return)) != 0)
    {
        samp->freeReference();
        throw n_u::IOException(getName(), "S100 init return packet", 
			       "doesn't match");
    }
    if (init_return->chksum.value() != expected_return.chksum.value()) {
        samp->freeReference();
        throw n_u::IOException(getName(), "S100 init return packet", 
			       "checksum doesn't match");
    }
    samp->freeReference();

    setMessageLength(_packetLen);
    setMessageParameters();
}

bool SPP100_Serial::process(const Sample* samp, list<const Sample*>& results)
	throw()
{
    if (! appendDataAndFindGood(samp))
      return false;

    /*
     * Copy the good record into our DMT100_blk struct.
     */
    DMT100_blk inRec;

    ::memcpy(&inRec, _waitingData, _packetLen - 2);
    ::memcpy(&inRec.chksum, _waitingData + _packetLen - 2, 2);

    /*
     * Shift the remaining data in _waitingData to the head of the line
     */
    _nWaitingData -= _packetLen;
    ::memmove(_waitingData, _waitingData + _packetLen, _nWaitingData);

    /*
     * Create the output stuff
     */
    SampleT<float>* outs = getSample<float>(_noutValues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = (inRec.cabinChan[FREF_INDX].value() - 2048) * 4.882812e-3;
    *dout++ = (inRec.cabinChan[FTMP_INDX].value() - 2328) * 0.9765625;
    *dout++ = _range;
    *dout++ = inRec.rejDOF.value();
    *dout++ = inRec.rejAvgTrans.value();
    *dout++ = inRec.ADCoverflow.value();

#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif    
    for (int iout = 0; iout < _nChannels; ++iout)
      *dout++ = inRec.OPCchan[iout].value();

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
