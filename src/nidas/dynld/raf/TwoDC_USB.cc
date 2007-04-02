/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoDC_USB.cc $

 ******************************************************************
*/


#include <nidas/linux/usbtwod/usbtwod.h>
#include <nidas/dynld/raf/TwoDC_USB.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,TwoDC_USB)

const n_u::EndianConverter* TwoDC_USB::toLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

const long long TwoDC_USB::_syncMask = 0xAAAAAAA000000000LL;


TwoDC_USB::TwoDC_USB()
{
  cerr << __PRETTY_FUNCTION__ << endl;

}

TwoDC_USB::~TwoDC_USB()
{
}

IODevice* TwoDC_USB::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* TwoDC_USB::buildSampleScanner()
{
    return new SampleScanner();
}


/*---------------------------------------------------------------------------*/
void TwoDC_USB::open(int flags) throw(n_u::IOException)
{
  DSMSensor::open(flags);

  cerr << __PRETTY_FUNCTION__ << "open-begin" << endl;

  // Shut the probe down until a valid TAS comes along.
  sendTrueAirspeed(0.0);

  _rtFeed = nidas::core::DerivedDataReader::getInstance();
  _rtFeed->addClient(this);

  cerr << __PRETTY_FUNCTION__ << "open-end" << endl;
}

void TwoDC_USB::close() throw(n_u::IOException)
{
  _rtFeed->removeClient(this);
  DSMSensor::close();
}

/*---------------------------------------------------------------------------*/
bool TwoDC_USB::process(const Sample * samp, list<const Sample *>& results)
        throw()
{
  assert(sizeof(long long) == 8);

  const unsigned char * input =
		(const unsigned char *)samp->getConstVoidDataPtr();

  // We will compute 1 value, a count of particles.
  size_t nvalues = 1;
  SampleT<float>* outs = getSample<float>(nvalues);

  outs->setTimeTag(samp->getTimeTag());
  outs->setId(getId() + 1);	// +1 ??

  float * dout = outs->getDataPtr();

  int cnt = 0;
  for (int i = 0; i < 512; ++i)
  {
    long long value = toLittle->longlongValue(&input[i*sizeof(long long)]);

    if ((value & _syncMask) == _syncMask)
      ++cnt;
  }

  *dout = cnt;

  return true;
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::fromDOMElement(const xercesc::DOMElement * node)
	throw(n_u::InvalidParameterException)
{
  DSMSensor::fromDOMElement(node);

  const Parameter *p;

  // Acquire probe diode/pixel resolution (in micrometers) for tas encoding.
  p = getParameter("RESOLUTION");
  if (!p)
    throw n_u::InvalidParameterException(getName(), "RESOLUTION","not found");
  _resolution = (int)p->getNumericValue(0);

  cerr << __PRETTY_FUNCTION__ << "fromDOMElement-end" << endl;
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
std::cerr << "tas " << s->getTrueAirspeed() << std::endl;
  sendTrueAirspeed(s->getTrueAirspeed());
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::sendTrueAirspeed(float tas)
{
  unsigned char tx_tas[3], ntap, nmsec, ndiv;

  /* Notes from Mike Spowart:
   *
   * Note below at the bottom that I send a 3 byte packet that is stuffed
   * with memcpy(s). The three bytes are ndiv, ntap, and nmsec.
   * Note that ndiv is presently set always to zero, but I wanted to reserve
   * the option to change it in the future (it was zero for the USB1.1 probe
   * but might be higher for the USB2.0 probe). Then the ntap byte is
   * determined by the tas, where tas->freq->ntap. Finally, nmsec is
   * sent to the probe as a number between 1 and 10 and is the count
   * of 10 Hz transmissions. 
   *
   * The nmsec byte will not be used in the fast probe. You can set it to any
   * value.  The TAS clock frequency is set entirely by ntap.
   * The rate at which TAS is sent does not matter at all to me. Whenever I
   * receive a new EP1OUT packet I will adjust the TAS clock. 
   */
  nmsec = 0;
  ndiv = 0;

  float freq = (float)(1.0e3 * (double)tas/(double)_resolution);
  if (freq <= 1000.0)
    ntap = (unsigned char)(0.286 * freq - 144.4);
  else if (freq > 2500.0)
    ntap = (unsigned char)(0.0015 * freq + 225.88);
  else
    ntap = (unsigned char)(0.0323 * freq + 111.5);

  tx_tas[0] = ntap;
  tx_tas[1] = ndiv;
  tx_tas[2] = nmsec;

  try
  {
    ioctl(USB2D_SET_TAS, tx_tas, 3);
  }
  catch (const n_u::IOException& e)
  {
    n_u::Logger::getInstance()->log(LOG_WARNING,"%s", e.what());
  }
}
