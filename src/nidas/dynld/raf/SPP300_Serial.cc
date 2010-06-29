/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3823 $

    $LastChangedDate: 2007-04-20 04:31:10 -0600 (Fri, 20 Apr 2007) $

    $LastChangedRevision: 3823 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SPP300_Serial.cc $

*/

#include <nidas/dynld/raf/SPP300_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SPP300_Serial)

const size_t SPP300_Serial::FREF_INDX = 4;
const size_t SPP300_Serial::FTMP_INDX = 7;


SPP300_Serial::SPP300_Serial(): SppSerial("SPP300")
{
    //
    // Make sure we got compiled with the packet structs packed appropriately.
    // If any of these assertions fails, then we can't just memcpy() between
    // the actual DMT packets and these structs, and we count on being able to
    // do that...
    //
    char* headPtr;
    char* chksumPtr;
    
    Init300_blk init;
    headPtr = (char*)&init;
    chksumPtr = (char*)&(init.chksum);
    assert((chksumPtr - headPtr) == (_InitPacketSize - 2));
    
    _nChannels = MAX_CHANNELS; // use a packet length containing all channels
    DMT300_blk data;
    headPtr = (char*)&data;
    chksumPtr = (char*)&(data.chksum);
    assert((chksumPtr - headPtr) == (packetLen() - 2));
    _nChannels = 0; // back to zero until it gets set via configuration

    //
    // Model number is fixed
    //
    _model = 300;

    //
    // This number should match the housekeeping added in ::process, so that
    // an output sample of the correct size is created.
    //
    _nHskp = 2;
}


void SPP300_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SppSerial::fromDOMElement(node);

    const Parameter *p;

    p = getParameter("THRESHOLD");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "THRESHOLD","not found");
    _triggerThreshold = (unsigned short)p->getNumericValue(0);

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DOF_REJ","not found");
    _dofReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("DIVISOR_FLAG");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DIVISOR_FLAG","not found");
    _divFlag = (unsigned short)p->getNumericValue(0);
}

void SPP300_Serial::sendInitString() throw(n_u::IOException)
{
    Init300_blk setup_pkt;

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    PackDMT_UShort(setup_pkt.trig_thresh, _triggerThreshold);
    PackDMT_UShort(setup_pkt.chanCnt, _nChannels);
    PackDMT_UShort(setup_pkt.dofRej, _dofReject);
    PackDMT_UShort(setup_pkt.range, _range);
    PackDMT_UShort(setup_pkt.avTranWe, _avgTransitWeight);
    PackDMT_UShort(setup_pkt.divFlag, _divFlag);

    for (int i = 0; i < _nChannels; i++)
        PackDMT_UShort(setup_pkt.OPCthreshold[i], _opcThreshold[i]);

    // exclude chksum from the computation
    PackDMT_UShort(setup_pkt.chksum, 
		   computeCheckSum((unsigned char*)&setup_pkt,
				   _InitPacketSize - 2));
    sendInitPacketAndCheckAck(&setup_pkt, _InitPacketSize);

    try {
        setMessageParameters(packetLen(),"",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"send init",e.what());
    }
}

bool SPP300_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if (! appendDataAndFindGood(samp))
      return false;

    /*
     * Copy the good record into our DMT300_blk struct.
     */
    DMT300_blk inRec;

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

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[FREF_INDX]) - 2048) * 
        4.882812e-3;
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[FTMP_INDX]) - 2328) * 
        0.9765625;
//    *dout++ = _range;
//    *dout++ = UnpackDMT_ULong(inRec.rejDOF);
//    *dout++ = UnpackDMT_ULong(inRec.ADCoverflow);

#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif    
    for (int iout = 0; iout < _nChannels; ++iout)
      *dout++ = UnpackDMT_ULong(inRec.OPCchan[iout]);

    // Compute DELTAT.
    int thisTime = samp->getTimeTag() / USECS_PER_MSEC;
    if (_prevTime == -1)
        _prevTime = thisTime;
    if (_outputDeltaT)
        *dout++ = thisTime - _prevTime;
    _prevTime = thisTime;

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
