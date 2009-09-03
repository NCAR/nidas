/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/raf/SppSerial.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

//NIDAS_CREATOR_FUNCTION_NS(raf,SppSerial)

SppSerial::~SppSerial()
{
    if (_totalRecordCount > 0) {
        cerr << "SppSerial::" << _probeName << ": " << _skippedRecordCount <<
		" records skipped of " << _totalRecordCount << " records for " <<
		((float)_skippedRecordCount/_totalRecordCount) * 100 << "% loss.\n";
    }
}

SppSerial::SppSerial(const std::string & probe) : DSMSerialSensor(),
  _probeName(probe),
  _range(0),
  _dataType(FixedLength),
  _checkSumErrorCnt(0),
  _waitingData(0),
  _nWaitingData(0),
  _skippedBytes(0),
  _skippedRecordCount(0),
  _totalRecordCount(0),
  _sampleRate(1),
  _outputDeltaT(false),
  _prevTime(-1)
{
  // If these aren't true, we're screwed!
  assert(sizeof(DMT_UShort) == 2);
  assert(sizeof(DMT_ULong) == 4);
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

void SppSerial::validate()
    throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::validate();

    _noutValues = 0;
    for (SampleTagIterator ti = getSampleTagIterator() ; ti.hasNext(); )
    {
        const SampleTag* stag = ti.next();

        VariableIterator vi = stag->getVariableIterator();
        for ( ; vi.hasNext(); )
        {
            const Variable* var = vi.next();
            _noutValues += var->getLength();
            if (var->getName().compare(0, 6, "DELTAT") == 0) {
                _outputDeltaT = true;
                ++_nHskp;
            }
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
    
    _waitingData = new unsigned char[2 * packetLen()];
    _nWaitingData = 0;
}

void SppSerial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);

    _sampleRate = (int)rint(getPromptRate());

    const Parameter *p;

    p = getParameter("NCHANNELS");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "NCHANNELS", "not found");
    _nChannels = (int)p->getNumericValue(0);

    p = getParameter("RANGE");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "RANGE", "not found");
    _range = (unsigned short)p->getNumericValue(0);

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "AVG_TRANSIT_WGT", "not found");
    _avgTransitWeight = (unsigned short)p->getNumericValue(0);

    p = getParameter("CHAN_THRESH");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "CHAN_THRESH", "not found");
    if (p->getLength() != _nChannels)
        throw n_u::InvalidParameterException(getName(), "CHAN_THRESH", 
					     "not NCHANNELS long ");
    for (int i = 0; i < p->getLength(); ++i)
        _opcThreshold[i] = (unsigned short)p->getNumericValue(i);

    const list<const SampleTag*> tags = getSampleTags();
    if (tags.size() != 1)
          throw n_u::InvalidParameterException(getName(), "sample", 
              "must be one <sample> tag for this sensor");

}


void SppSerial::sendInitPacketAndCheckAck(void * setup_pkt, int len)
{   
    std::string eType("SppSerial init-ack");

    // The initialization response is two bytes 0x0606 with
    // no separator.
    setMessageLength(1);
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

    setMessageLength(2);
    setMessageParameters(); // does the ioctl

    n_u::UTime twrite;
    
    ILOG(("%s: sending packet, length=%d",getName().c_str(),len));
    write(setup_pkt, len);

    //
    // Get the response
    //

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    readBuffer(MSECS_PER_SEC * 5);

    Sample* samp = nextSample();
    if (!samp)
        throw n_u::IOException(getName(), eType, "not read.");

    n_u::UTime tread;
    cerr << "Received init Ack after " <<
        (tread.toUsecs() - twrite.toUsecs()) << " usecs" << endl;

    if (samp->getDataByteLength() != 2) {
        ostringstream ost;
        ost << "wrong size=" << samp->getDataByteLength() << " expected=2" << endl;
        samp->freeReference();
        throw n_u::IOException(getName(), eType, ost.str());
    }

    // pointer to the returned data
    short * init_return = (short *) samp->getVoidDataPtr();

    // 
    // see if we got the expected response
    //
    if (*init_return != 0x0606)
    {
        samp->freeReference();
	ILOG(("%s: received packet, data= %#04hx, expected 0x0606",getName().c_str(),*init_return));
        throw n_u::IOException(getName(), eType, "not expected return of 0x0606.");
    }
    samp->freeReference();
}


int SppSerial::appendDataAndFindGood(const Sample* samp)
{
    if ((signed)samp->getDataByteLength() != packetLen()) 
      return false;
    
    /*
     * Add the sample to our waiting data buffer
     */
    assert(_nWaitingData <= packetLen());
    ::memcpy(_waitingData + _nWaitingData, samp->getConstVoidDataPtr(), 
	     packetLen());
    _nWaitingData += packetLen();

    /*
     * Hunt in the waiting data until we find a packetLen() sized stretch
     * where the last two bytes are a good checksum for the rest or match
     * the expected record delimiter.  Most of the time, we should find it 
     * on the first pass through the loop.
     */
    bool foundRecord = 0;
    for (int offset = 0; offset <= (_nWaitingData - packetLen()); offset++) {
      unsigned char *input = _waitingData + offset;
      DMT_UShort packetCheckSum;
      ::memcpy(&packetCheckSum, input + packetLen() - 2, 2);
      switch (_dataType) 
      {
       case Delimited:
	foundRecord = (UnpackDMT_UShort(packetCheckSum) == _recDelimiter);
	break;
       case FixedLength:
	foundRecord = (computeCheckSum(input, packetLen() - 2) == 
		       UnpackDMT_UShort(packetCheckSum));
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
//	  cerr << "SppSerial::appendDataAndFind(" << _probeName << ") skipped " <<
//		_skippedBytes << " bytes to find a good " << packetLen() << "-byte record.\n";
	  _skippedBytes = 0;
          _skippedRecordCount++;
	}

        _totalRecordCount++;
	return true;
      }
    }

    /*
     * If we didn't find a good record, keep the last packetLen()-1 bytes of
     * the waiting data and wait for the next blob of input.
     */
    int nDrop = _nWaitingData - (packetLen() - 1);
    _nWaitingData -= nDrop;
    ::memmove(_waitingData, _waitingData + nDrop, _nWaitingData);

    _skippedBytes += nDrop;

    return false;
}

void SppSerial::addSampleTag(SampleTag * tag)
        throw(n_u::InvalidParameterException)
{
    DSMSensor::addSampleTag(tag);
}

