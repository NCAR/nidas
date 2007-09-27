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
#include <nidas/util/UTime.h>
#include <nidas/util/IOTimeoutException.h>

#include <cmath>
#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CDP_Serial)

const size_t CDP_Serial::FREF_INDX = 4;
const size_t CDP_Serial::FTMP_INDX = 7;


CDP_Serial::CDP_Serial(): SppSerial(), _sampleRate(1)
{
  _model = 100;
  // This number should match the housekeeping added in ::process, so that
  // an output sample of the correct size is created.
  _nHskp = 6;
}


void CDP_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SppSerial::fromDOMElement(node);

    //
    // Fixed record delimiter (for now?)
    //
    _dataType = Delimited;
    _recDelimiter = 0xffff;

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

    memset((void *)&setup_pkt, 0, sizeof(setup_pkt));

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    setup_pkt.trig_thresh.putValue(_triggerThreshold);
    setup_pkt.transRej.putValue(_transitReject);
    setup_pkt.chanCnt.putValue((unsigned short)_nChannels);
    setup_pkt.dofRej.putValue(_dofReject);
    setup_pkt.range.putValue(_range);
    setup_pkt.avTranWe.putValue(_avgTransitWeight);
    setup_pkt.attAccept.putValue(_attAccept);
    setup_pkt.divFlag.putValue(_divFlag);
    setup_pkt.ct_method.putValue(_ctMethod);

    for (int i = 0; i < _nChannels; i++)
        setup_pkt.OPCthreshold[i].putValue(_opcThreshold[i]);

    // exclude chksum from the computation (but since it is zero
    // at this point, it doesn't really matter).
    setup_pkt.chksum.putValue(computeCheckSum((unsigned char*)&setup_pkt,
					      sizeof(setup_pkt) - 2));

    // The initialization response is two bytes 0x0606 with
    // no separator.
    setMessageLength(2);
    setMessageSeparator("");
    setMessageParameters(); // does the ioctl

    // clear whatever junk may be in the buffer til a timeout
    try {
        for (;;) {
            readBuffer(MSECS_PER_SEC / 100);
            clearBuffer();
        }
    }
    catch (const n_u::IOTimeoutException& e) {}

    n_u::UTime twrite;
    write(&setup_pkt, sizeof(setup_pkt));

    //
    // Get the response
    //

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    readBuffer(MSECS_PER_SEC * 5);

    Sample* samp = nextSample();
    if (!samp) 
        throw n_u::IOException(getName(),
            "CDP init return packet","not read");

    n_u::UTime tread;
    cerr << "received init packet after " <<
        (tread.toUsecs() - twrite.toUsecs()) << " usecs" << endl;

    if (samp->getDataByteLength() != 2) {
        ostringstream ost;
        ost << "CDP init return packet, wrong size=" <<
            samp->getDataByteLength() <<
            " expected=2" << endl;
        samp->freeReference();
        throw n_u::IOException(getName(),"sendInitString",ost.str());
    }

    // pointer to the returned data
    short * init_return = (short *) samp->getVoidDataPtr();

    // 
    // see if we got the expected response
    //
    if (*init_return != 0x0606)
    {
        samp->freeReference();
        throw n_u::IOException(getName(), "CDP init return packet",
			       "doesn't match");
    }
    samp->freeReference();

    // Now we're running. Set the message parameters appropriate for
    // normal operation.
    setMessageSeparator("\xff\xff");
    setMessageSeparatorAtEOM(true);
    setMessageLength(_packetLen - 2);       // subtract off length of separator
    setMessageParameters(); // does the ioctl
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
      *dout++ = inRec.OPCchan[iout].value() * _sampleRate;

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}

void CDP_Serial::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{
  DSMSensor::addSampleTag(tag);
  _sampleRate = (int)rint(tag->getRate());
}
