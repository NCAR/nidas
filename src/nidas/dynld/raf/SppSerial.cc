/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/raf/SppSerial.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

//NIDAS_CREATOR_FUNCTION_NS(raf,SppSerial)

SppSerial::SppSerial() : DSMSerialSensor(),
  _range(0),
  _dataType(FixedLength),
  _checkSumErrorCnt(0),
  _waitingData(0),
  _nWaitingData(0),
  _skippedBytes(0)
{
}

unsigned short SppSerial::computeCheckSum(const unsigned char * pkt, int len)
{
    unsigned short sum = 0;
    // Compute the checksum of a series of chars
    // Sum the byte count and data bytes;
    for (int j = 0; j < len; j++) 
      sum += (unsigned short)pkt[j];
    return sum;
}

void SppSerial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);

    const Parameter *p;

    p = getParameter("NCHANNELS");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "NCHANNELS", "not found");
    _nChannels = (int)p->getNumericValue(0);
    _packetLen = calculatePacketLen(_nChannels);

    p = getParameter("RANGE");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "RANGE", "not found");
    _range = (unsigned short)p->getNumericValue(0);

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "AVG_TRANSIT_WGT", 
					   "not found");
    _avgTransitWeight = (unsigned short)p->getNumericValue(0);

    p = getParameter("CHAN_THRESH");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "CHAN_THRESH", 
					   "not found");
    if (p->getLength() != _nChannels)
        throw n_u::InvalidParameterException(getName(), "CHAN_THRESH", 
					     "not NCHANNELS long ");
    for (int i = 0; i < p->getLength(); ++i)
        _opcThreshold[i] = (unsigned short)p->getNumericValue(i);

    const set<const SampleTag*> tags = getSampleTags();
    if (tags.size() != 1)
          throw n_u::InvalidParameterException(getName(), "sample", 
              "must be one <sample> tag for this sensor");

    _noutValues = 0;
    for (SampleTagIterator ti = getSampleTagIterator() ; ti.hasNext(); )
    {
        const SampleTag* stag = ti.next();

        VariableIterator vi = stag->getVariableIterator();
        for ( ; vi.hasNext(); )
        {
            const Variable* var = vi.next();
            _noutValues += var->getLength();
        }
    }
#ifdef ZERO_BIN_HACK
    /*
     * We'll be adding a bogus zeroth bin to the data to match historical 
     * behavior. Remove all traces of this after the netCDF file refactor.
     */
    if (_noutValues != _nChannels + _nHskp + 1) {
        ostringstream ost;
        ost << "total length of variables should be " << 
	  (_nChannels + _nHskp + 1) << " rather than " << _noutValues << ".\n";
          throw n_u::InvalidParameterException(getName(), "sample",
					       ost.str());
    }
#else
    if (_noutValues != _nChannels + _nHskp) {
        ostringstream ost;
        ost << "total length of variables should be " << 
	  (_nChannels + _nHskp) << " rather than " << _noutValues << ".\n";
          throw n_u::InvalidParameterException(getName(), "sample",
					       ost.str());
    }
#endif

    /*
     * Allocate a new buffer for yet-to-be-handled data.  We get enough space
     * to hold up to two full samples.
     */
    if (_waitingData)
      delete[] _waitingData;
    
    _waitingData = new unsigned char[2 * _packetLen];
    _nWaitingData = 0;

}


int SppSerial::appendDataAndFindGood(const Sample* samp) {
    if ((signed)samp->getDataByteLength() != _packetLen) 
      return false;
    
    /*
     * Add the sample to our waiting data buffer
     */
    assert(_nWaitingData <= _packetLen);
    ::memcpy(_waitingData + _nWaitingData, samp->getConstVoidDataPtr(), 
	     _packetLen);
    _nWaitingData += _packetLen;

    /*
     * Hunt in the waiting data until we find a _packetLen sized stretch
     * where the last two bytes are a good checksum for the rest or match
     * the expected record delimiter.  Most of the time, we should find it 
     * on the first pass through the loop.
     */
    bool foundRecord = 0;
    for (int offset = 0; offset <= (_nWaitingData - _packetLen); offset++) {
      unsigned char *input = _waitingData + offset;
      DMT_UShort packetCheckSum;
      ::memcpy(&packetCheckSum, input + _packetLen - 2, 2);
      switch (_dataType) 
      {
       case Delimited:
	foundRecord = (packetCheckSum.value() == _recDelimiter);
	break;
       case FixedLength:
	foundRecord = 
	  (computeCheckSum(input, _packetLen - 2) == packetCheckSum.value());
	break;
      }
      
      if (foundRecord)
      {
	/*
	 * Drop data so that the good record is at the beginning of
	 * _waitingData
	 */
	if (offset > 0)	{
	  _nWaitingData -= offset;
	  ::memmove(_waitingData, _waitingData + offset, _nWaitingData);

	  _skippedBytes += offset;
	}

	if (_skippedBytes) {
	  cerr << "SppSerial::appendDataAndFind skipped " << _skippedBytes << 
	    " bytes to find a good " << _packetLen << "-byte record.\n";
	  _skippedBytes = 0;
	}

	return true;
      }
    }

    /*
     * If we didn't find a good record, keep the last _packetLen-1 bytes of
     * the waiting data and wait for the next blob of input.
     */
    int nDrop = _nWaitingData - (_packetLen - 1);
    _nWaitingData -= nDrop;
    ::memmove(_waitingData, _waitingData + nDrop, _nWaitingData);

    _skippedBytes += nDrop;

    return false;
}

