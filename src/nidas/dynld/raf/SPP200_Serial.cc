/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-24 07:51:51 -0600 (Mon, 24 Jul 2006) $

    $LastChangedRevision: 3446 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SPP200_Serial.cc $

*/

#include <nidas/dynld/raf/SPP200_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SPP200_Serial)


SPP200_Serial::SPP200_Serial(): SppSerial()
{
  // struct is packed to incorrect length for 16 bit probe.  Compensate.
  _initPacketLen = sizeof(Init200_blk) - 2;
}


void SPP200_Serial::fromDOMElement(const xercesc::DOMElement* node)
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

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "AVG_TRANSIT_WGT","not found");
    _avgTransitWeight = (unsigned short)p->getNumericValue(0);

    p = getParameter("CHAN_THRESH");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "CHAN_THRESH","not found");
    if (p->getLength() != _nChannels)
        throw n_u::InvalidParameterException(getName(),
              "CHAN_THRESH","not NCHANNELS long ");
    for (int i = 0; i < p->getLength(); ++i)
        _opcThreshold[i] = (unsigned short)p->getNumericValue(i);

    _packetLen = sizeof(DMT200_blk);
    _packetLen -= (MAX_CHANNELS - _nChannels) * sizeof(long);

    // I have no explanation, but it's in the ADS2 code also.  cjw.
    if (_nChannels == 30)
      _packetLen -= 2;

    const set<const SampleTag*> tags = getSampleTags();
    if (tags.size() != 1)
          throw n_u::InvalidParameterException(getName(),"sample",
              "must be one <sample> tag for this sensor");

    _noutValues = 0;
    for (SampleTagIterator ti = getSampleTagIterator() ; ti.hasNext(); ) {
        const SampleTag* stag = ti.next();
        _sampleId = stag->getId();

        VariableIterator vi = stag->getVariableIterator();
        for ( ; vi.hasNext(); ) {
            const Variable* var = vi.next();
            _noutValues += var->getLength();
        }
    }

    // This logic should match what is in ::process, so that
    // an output sample of the correct size is created.
    if (_noutValues != _nChannels + 8) {
        ostringstream ost;
        ost << "total length of variables should be " << (_nChannels + 8);
          throw n_u::InvalidParameterException(getName(),"sample",ost.str());
    }
}

void SPP200_Serial::sendInitString() throw(n_u::IOException)
{
    memset((void *)&_setup_pkt, 0, sizeof(_setup_pkt));

    _setup_pkt.esc = 0x1b;
    _setup_pkt.id = 0x01;
    _setup_pkt.model = toLittle->ushortValue(200);
    _setup_pkt.trig_thresh = toLittle->ushortValue(80);
    _setup_pkt.chanCnt = toLittle->ushortValue((unsigned short)_nChannels);
    _setup_pkt.range = toLittle->ushortValue(_range);
    _setup_pkt.avTranWe = toLittle->ushortValue(_avgTransitWeight);
    _setup_pkt.divFlag = toLittle->ushortValue(0x02);
    _setup_pkt.max_width = toLittle->ushortValue(0xFFFF);

    for (int i = 0; i < _nChannels; i++)
        _setup_pkt.OPCthreshold[i] = toLittle->ushortValue(_opcThreshold[i]);

    _setup_pkt.chksum = toLittle->ushortValue(
	computeCheckSum((unsigned char*)&_setup_pkt, _initPacketLen));

    if (getMessageLength() != sizeof(Response200_blk)) {
        setMessageLength(sizeof(Response200_blk));
        setMessageParameters();
    }

char t[20], *p = (char *)&_setup_pkt;
for (int k = 0; k < _initPacketLen; ++k)
{
  sprintf(t, "0x%02X ", p[k]);
  cerr << t;
}
cerr << endl;

    // clear whatever junk may be in the buffer til a timeout
    try {
        for (;;) {
            readBuffer(MSECS_PER_SEC / 100);
            clearBuffer();
        }
    }
    catch (const n_u::IOTimeoutException& e) {}

    write(&_setup_pkt, _initPacketLen);

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    size_t rlen = readBuffer(MSECS_PER_SEC / 2);
    PLOG(("readBuffer, rlen=") << rlen);

    Sample* samp = nextSample();
    if (!samp) 
        throw n_u::IOException(getName(), "S200 init return packet","not read");
    PLOG(("returned sample length=") << samp->getDataByteLength());

    if (samp->getDataByteLength() != sizeof(Response200_blk)) {
        ostringstream ost;
        ost << "S200 init return packet, wrong size=" <<
            samp->getDataByteLength() <<
            " expected=" << sizeof(Response200_blk) << endl;
        samp->freeReference();
        throw n_u::IOException(getName(),"sendInitString",ost.str());
    }

    // Fill this in from _setup_pkt.
    Response200_blk expected_return;
    ::memcpy(&expected_return, &_setup_pkt, 4);
    toLittle->ushortCopy(0x105, &expected_return.firmware); 
    PLOG(("expected firmware=") << hex << expected_return.firmware << dec);
//    expected_return.firmware = 0x105;
    ::memcpy(((ushort *)&expected_return)+3, ((ushort *)&_setup_pkt)+2, _initPacketLen-4);

    //
    Response200_blk* init_return = (Response200_blk*) samp->getVoidDataPtr();

//char t[20], *p = (char *)init_return;
p = (char *)init_return;
for (int k = 0; k < 106; ++k)
{
  sprintf(t, "0x%02X ", p[k]);
  cerr << t;
}
cerr << endl;

    if (::memcmp(init_return, &expected_return, sizeof(Response200_blk)) != 0)
    {
        samp->freeReference();
        throw n_u::IOException(getName(), "S200 init return packet","doesn't match");
    }
    samp->freeReference();

    setMessageLength(_packetLen);
    setMessageParameters();
}


bool SPP200_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if ((signed)samp->getDataByteLength() != _packetLen) return false;

    const DMT200_blk *input = (DMT200_blk *) samp->getConstVoidDataPtr();

    SampleT<float>* outs = getSample<float>(_noutValues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    for (int iout = 0; iout < 8; ++iout)
      *dout++ = input->cabinChan[iout];

    for (int iout = 0; iout < _nChannels; ++iout)
      *dout++ = input->OPCchan[iout];

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
