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


CDP_Serial::CDP_Serial(): SppSerial(), _checkSumErrorCnt(0), _sampleRate(1)
{
  _model = 100;
}


void CDP_Serial::fromDOMElement(const xercesc::DOMElement* node)
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

    p = getParameter("TRANSIT_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "TRANSIT_REJ","not found");
    _transitReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DOF_REJ","not found");
    _dofReject = (unsigned short)p->getNumericValue(0);

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "AVG_TRANSIT_WGT","not found");
    _avgTransitWeight = (unsigned short)p->getNumericValue(0);

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

    p = getParameter("CHAN_THRESH");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "CHAN_THRESH","not found");
    if (p->getLength() != _nChannels)
        throw n_u::InvalidParameterException(getName(),
              "CHAN_THRESH","not NCHANNELS long ");
    for (int i = 0; i < p->getLength(); ++i)
        _opcThreshold[i] = (unsigned short)p->getNumericValue(i);

    _packetLen = sizeof(DMT100_blk);
    _packetLen -= (MAX_CHANNELS - _nChannels) * sizeof(long);

    /* According to the manual, packet lens are 76, 116, 156 and 196
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
    const int nHousekeep = 6;
    if (_noutValues != _nChannels + nHousekeep) {
        ostringstream ost;
        ost << "total length of variables should be " << (_nChannels + nHousekeep);
          throw n_u::InvalidParameterException(getName(),"sample",ost.str());
    }
}

void CDP_Serial::sendInitString() throw(n_u::IOException)
{
    Init100_blk setup_pkt;

    memset((void *)&setup_pkt, 0, sizeof(setup_pkt));

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    setup_pkt.trig_thresh = toLittle->ushortValue(_triggerThreshold);
    setup_pkt.transRej = toLittle->ushortValue(_transitReject);
    setup_pkt.chanCnt = toLittle->ushortValue((unsigned short)_nChannels);
    setup_pkt.dofRej = toLittle->ushortValue(_dofReject);
    setup_pkt.range = toLittle->ushortValue(_range);
    setup_pkt.avTranWe = toLittle->ushortValue(_avgTransitWeight);
    setup_pkt.attAccept = toLittle->ushortValue(_attAccept);
    setup_pkt.divFlag = toLittle->ushortValue(_divFlag);
    setup_pkt.ct_method = toLittle->ushortValue(_ctMethod);

    for (int i = 0; i < _nChannels; i++)
        setup_pkt.OPCthreshold[i] = toLittle->ushortValue(_opcThreshold[i]);

    // struct is padded at end to modulus 4. We want unpadded length.
    int plen = (char*)(&setup_pkt.chksum + 1) - (char*)&setup_pkt;

    // exclude chksum from the computation (but since it is zero
    // at this point, it doesn't really matter).
    setup_pkt.chksum = toLittle->ushortValue(
	computeCheckSum((unsigned char*)&setup_pkt,
            plen-sizeof(setup_pkt.chksum)));
/*
    if (getMessageLength() != sizeof(Response100_blk)) {
        setMessageLength(sizeof(Response100_blk));
        setMessageParameters();
    }
*/
    setMessageLength(2);
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
    write(&setup_pkt, plen);

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
    if (*init_return != 0x0606)
    {
        samp->freeReference();
        throw n_u::IOException(getName(), "CDP init return packet","doesn't match");
    }
    samp->freeReference();

    setMessageLength(_packetLen);
    setMessageParameters(); // does the ioctl
}

bool CDP_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if ((signed)samp->getDataByteLength() != _packetLen) return false;

    const DMT100_blk *input = (DMT100_blk *) samp->getConstVoidDataPtr();

    unsigned short packetCheckSum = ((unsigned short *)input)[(_packetLen/2)-1];

    if (packetCheckSum != 65535 &&
	computeCheckSum((unsigned char *)input, _packetLen - 2) != packetCheckSum)
    {
        ++_checkSumErrorCnt;

        if (_checkSumErrorCnt < 5)
            cerr << "CDP::process, bad checksum!  Sent = " << packetCheckSum
		<< ", computed = "
		<< computeCheckSum((unsigned char *)input, _packetLen - 2)
		<< std::endl;

        if (_checkSumErrorCnt == 1000)
        {
            cerr << "CDP::process, bad checksum, repeated "
		<< _checkSumErrorCnt << " times.\n";
            _checkSumErrorCnt = 0;
        }
    }

    SampleT<float>* outs = getSample<float>(_noutValues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = (input->cabinChan[FREF_INDX] - 2048) * 4.882812e-3;
    *dout++ = (input->cabinChan[FTMP_INDX] - 2328) * 0.9765625;
    *dout++ = _range;
    *dout++ = fuckedUpLongFlip((char *)&input->rejDOF);
    *dout++ = fuckedUpLongFlip((char *)&input->rejAvgTrans);
    *dout++ = fuckedUpLongFlip((char *)&input->ADCoverflow);

    // DMT fucked up the word count.  Re-align long data on mod 4 boundary.
    const char * p = (char *)input->OPCchan - 2;

    for (int iout = 0; iout < _nChannels; ++iout)
    {
      *dout++ = fuckedUpLongFlip(p) * _sampleRate;
      p += sizeof(unsigned long);
    }

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
