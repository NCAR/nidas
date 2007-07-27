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


SPP300_Serial::SPP300_Serial(): SppSerial()
{
  _model = 300;
}


void SPP300_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);

    const Parameter *p;

    p = getParameter("NCHANNELS");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "NCHANNELS","not found");
    _nChannels = (int)p->getNumericValue(0);

    p = getParameter("RANGE");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "RANGE","not found");
    _range = (unsigned short)p->getNumericValue(0);

    p = getParameter("THRESHOLD");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "THRESHOLD","not found");
    _triggerThreshold = (unsigned short)p->getNumericValue(0);

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DOF_REJ","not found");
    _dofReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "AVG_TRANSIT_WGT","not found");
    _avgTransitWeight = (unsigned short)p->getNumericValue(0);

    p = getParameter("DIVISOR_FLAG");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DIVISOR_FLAG","not found");
    _divFlag = (unsigned short)p->getNumericValue(0);

    p = getParameter("MAX_WIDTH");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "MAX_WIDTH","not found");
    _maxWidth = (unsigned short)p->getNumericValue(0);

    p = getParameter("CHAN_THRESH");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "CHAN_THRESH","not found");
    if (p->getLength() != _nChannels)
        throw n_u::InvalidParameterException(getName(),
              "CHAN_THRESH","not NCHANNELS long ");
    for (int i = 0; i < p->getLength(); ++i)
        _opcThreshold[i] = (unsigned short)p->getNumericValue(i);

    _packetLen = sizeof(DMT300_blk);
    _packetLen -= (MAX_CHANNELS - _nChannels) * sizeof(long);

    /* According to the manual, packet lens are 74, 114, 154 and 194
     * for 10, 20, 30 and 40 channels respectively.
     */
    _packetLen -= 4;

    const set<const SampleTag*> tags = getSampleTags();
    if (tags.size() != 1)
          throw n_u::InvalidParameterException(getName(),"sample",
              "must be one <sample> tag for this sensor");

    _noutValues = 0;
    for (SampleTagIterator ti = getSampleTagIterator() ; ti.hasNext(); )
    {
        const SampleTag* stag = ti.next();
//        dsm_sample_id_t sampleId = stag->getId();

        VariableIterator vi = stag->getVariableIterator();
        for ( ; vi.hasNext(); )
        {
            const Variable* var = vi.next();
            _noutValues += var->getLength();
        }
    }

    // This logic should match what is in ::process, so that
    // an output sample of the correct size is created.
    static const int _nHskp = 2;
    if (_noutValues != _nChannels + _nHskp) {
        ostringstream ost;
        ost << "total length of variables should be " << (_nChannels + _nHskp);
          throw n_u::InvalidParameterException(getName(),"sample",ost.str());
    }
}

void SPP300_Serial::sendInitString() throw(n_u::IOException)
{
    Init300_blk setup_pkt;

    memset((void *)&setup_pkt, 0, sizeof(setup_pkt));

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    setup_pkt.model = toLittle->ushortValue(_model);
    setup_pkt.trig_thresh = toLittle->ushortValue(_triggerThreshold);
    setup_pkt.chanCnt = toLittle->ushortValue((unsigned short)_nChannels);
    setup_pkt.dofRej = toLittle->ushortValue(_dofReject);
    setup_pkt.range = toLittle->ushortValue(_range);
    setup_pkt.avTranWe = toLittle->ushortValue(_avgTransitWeight);
    setup_pkt.divFlag = toLittle->ushortValue(_divFlag);
    setup_pkt.max_width = toLittle->ushortValue(_maxWidth);

    for (int i = 0; i < _nChannels; i++)
        setup_pkt.OPCthreshold[i] = toLittle->ushortValue(_opcThreshold[i]);

    // struct is padded at end to modulus 4. We want unpadded length.
    int plen = (char*)(&setup_pkt.chksum + 1) - (char*)&setup_pkt;

    // exclude chksum from the computation (but since it is zero
    // at this point, it doesn't really matter).
    setup_pkt.chksum = toLittle->ushortValue(
	computeCheckSum((unsigned char*)&setup_pkt,
            plen-sizeof(setup_pkt.chksum)));

    setMessageLength(1);
    setMessageSeparator("");
    setMessageParameters();

    // clear whatever junk may be in the buffer til a timeout
    try {
        for (;;) {
            readBuffer(MSECS_PER_SEC / 300);
            clearBuffer();
        }
    }
    catch (const n_u::IOTimeoutException& e) {}

    setMessageLength(sizeof(Response300_blk));
    setMessageParameters();

    write(&setup_pkt, plen);

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    readBuffer(MSECS_PER_SEC * 3);


    Sample* samp = nextSample();
    if (!samp) 
        throw n_u::IOException(getName(),
            "S300 init return packet","not read");


    if (samp->getDataByteLength() != sizeof(Response300_blk)) {
        ostringstream ost;
        ost << "S300 init return packet, wrong size=" <<
            samp->getDataByteLength() <<
            " expected=" << sizeof(Response300_blk) << endl;
        samp->freeReference();
        throw n_u::IOException(getName(),"sendInitString",ost.str());
    }

    // Probe echoes back a structure like the setup packet
    // but with a firmware field in the middle, and
    // a new checksum.
    Response300_blk expected_return;
    ::memcpy(&expected_return, &setup_pkt, 4);
    expected_return.firmware = toLittle->ushortValue(0x105);
    ::memcpy(&expected_return.trig_thresh, &setup_pkt.trig_thresh, plen-4);
    // on expected_return, plen excludes the chksum
    expected_return.chksum = toLittle->ushortValue(
	computeCheckSum((unsigned char*)&expected_return,plen));

    // pointer to the returned data
    Response300_blk* init_return = (Response300_blk*) samp->getVoidDataPtr();

    // 
    if (::memcmp(init_return, &expected_return, plen) != 0)
    {
        samp->freeReference();
        throw n_u::IOException(getName(), "S300 init return packet","doesn't match");
    }
    if (init_return->chksum != expected_return.chksum) {
        samp->freeReference();
        throw n_u::IOException(getName(), "S300 init return packet","checksum doesn't match");
    }
    samp->freeReference();

    setMessageLength(_packetLen);
    setMessageParameters();
}

bool SPP300_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if ((signed)samp->getDataByteLength() != _packetLen) return false;

    const DMT300_blk *input = (DMT300_blk *) samp->getConstVoidDataPtr();

    unsigned short packetCheckSum = ((unsigned short *)input)[(_packetLen/2)-1];

    if (computeCheckSum((unsigned char *)input, _packetLen - 2) != packetCheckSum)
      cerr << "SPP300::process, bad checksum!\n";

    SampleT<float>* outs = getSample<float>(_noutValues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = (input->cabinChan[FREF_INDX] - 2048) * 4.882812e-3;
    *dout++ = (input->cabinChan[FTMP_INDX] - 2328) * 0.9765625;
//    *dout++ = _range;
//    *dout++ = fuckedUpLongFlip((char *)&input->rejDOF);
//    *dout++ = fuckedUpLongFlip((char *)&input->ADCoverflow);

    // DMT fucked up the word count.  Re-align long data on mod 4 boundary.
    const char * p = (char *)input->OPCchan - 2;

    for (int iout = 0; iout < _nChannels; ++iout)
    {
      *dout++ = fuckedUpLongFlip(p);
      p += sizeof(unsigned long);
    }

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
