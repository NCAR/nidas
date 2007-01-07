/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-24 07:51:51 -0600 (Mon, 24 Jul 2006) $

    $LastChangedRevision: 3446 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SPP200_Serial.cc $

*/

#include <nidas/dynld/raf/SPP200_Serial.h>
#include <nidas/core/PhysConstants.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SPP200_Serial)

void SPP200_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);

    memset((void *)&_setup_pkt, 0, sizeof(_setup_pkt));

    _setup_pkt.id = 0x1b01;
    _setup_pkt.model = 200;
    _setup_pkt.trig_thresh = 80;
    _setup_pkt.divFlag = 0x02;
    _setup_pkt.max_width = 0xFFFF;

    const Parameter *p;

    p = getParameter("NCHANNELS");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "NCHANNELS","not found");
    _nChannels = _setup_pkt.chanCnt = (ushort)p->getNumericValue(0);

    p = getParameter("RANGE");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "RANGE","not found");
    _setup_pkt.range = (ushort)p->getNumericValue(0);

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "AVG_TRANSIT_WGT","not found");
    _setup_pkt.avTranWe = (ushort)p->getNumericValue(0);

    p = getParameter("CHAN_THRESH");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "CHAN_THRESH","not found");
    for (int i = 0; i < p->getLength(); ++i)
      _setup_pkt.OPCthreshold[i] = (ushort)p->getNumericValue(i);


    _packetLen = sizeof(DMT200_blk);
    _packetLen -= (MAX_CHANNELS - _setup_pkt.chanCnt) * sizeof(long);

    // I have no explanation, but it's in the ADS2 code also.  cjw.
    if (_setup_pkt.chanCnt == 30)
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
    Init200_blk init_return;

    write(&_setup_pkt, sizeof(_setup_pkt));

    setMessageLength(sizeof(Init200_blk));
    setMessageParameters();
    // half second timeout. Throws n_u::IOTimeoutException
    readBuffer(MSECS_PER_SEC / 2);

    Sample* samp = nextSample();
    if (!samp) 
        throw n_u::IOException(getName(), "S200 init return packet","not read");
    setMessageLength(_packetLen);
    setMessageParameters();

    cerr << "returned sample length=" << samp->getDataByteLength() << endl;

    if (samp->getDataByteLength() != sizeof(Init200_blk))
        throw n_u::IOException(getName(), "S200 init return packet","wrong size");

    if (memcmp(&init_return, &_setup_pkt, sizeof(Init200_blk)) != 0)
        throw n_u::IOException(getName(), "S200 init return packet","doesn't match");
}

bool SPP200_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if (samp->getDataByteLength() != _packetLen) return false;

    const DMT200_blk *input = (DMT200_blk *) samp->getConstVoidDataPtr();

    SampleT<float>* outs = getSample<float>(_noutValues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    for (size_t iout = 0; iout < 8; ++iout)
      *dout++ = input->cabinChan[iout];

    for (size_t iout = 0; iout < _nChannels; ++iout)
      *dout++ = input->OPCchan[iout];

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
