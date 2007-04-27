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

const long long TwoDC_USB::_syncWord = 0xAAAAAA0000000000LL;
const long long TwoDC_USB::_syncMask = 0xFFFFFF0000000000LL;


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
    return new SampleScanner((4104+8)*4);
}


/*---------------------------------------------------------------------------*/
void TwoDC_USB::open(int flags) throw(n_u::IOException)
{
  DSMSensor::open(flags);

  cerr << __PRETTY_FUNCTION__ << "open-begin" << endl;

  // Shut the probe down until a valid TAS comes along.
  sendTrueAirspeed(33.0);

  if (DerivedDataReader::getInstance())
	  DerivedDataReader::getInstance()->addClient(this);

  cerr << __PRETTY_FUNCTION__ << "open-end" << endl;
}

void TwoDC_USB::close() throw(n_u::IOException)
{
  DerivedDataReader::getInstance()->removeClient(this);
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

  // Count number of particles (sync words) in the record and return.
  int cnt = 0;
  for (int i = 0; i < 512; ++i)
  {
    long long value = toLittle->longlongValue(&input[i*sizeof(long long)]);

    if ((value & _syncMask) == _syncWord)
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
  _resolution = p->getNumericValue(0) * 1.0e-6;

  cerr << __PRETTY_FUNCTION__ << "fromDOMElement-end" << endl;
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
std::cerr << "tas " << s->getTrueAirspeed() << std::endl;
  if (!::isnan(s->getTrueAirspeed()))
      sendTrueAirspeed(s->getTrueAirspeed());
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::sendTrueAirspeed(float tas)
{
  Tap2D tx_tas;

  TASToTap2D(&tx_tas, tas, _resolution);

  try
  {
    ioctl(USB2D_SET_TAS, (void *)&tx_tas, 3);
  }
  catch (const n_u::IOException& e)
  {
    n_u::Logger::getInstance()->log(LOG_WARNING,"%s", e.what());
  }
}
