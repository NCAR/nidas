/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: dongl $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD_USB.cc $

 ******************************************************************
*/

#include <nidas/dynld/raf/TwoD_USB.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD_USB)

const n_u::EndianConverter * TwoD_USB::bigEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);


TwoD_USB::TwoD_USB() : _tasRate(1)
{
}

TwoD_USB::~TwoD_USB()
{
}

IODevice *TwoD_USB::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner *TwoD_USB::buildSampleScanner()
{   
    return new SampleScanner((4104 + 8) * 4);
}


/*---------------------------------------------------------------------------*/
void TwoD_USB::open(int flags) throw(n_u::IOException)
{
    DSMSensor::open(flags);

    // Shut the probe down until a valid TAS comes along.
    sendTrueAirspeed(33.0);

    // cerr << "SET_SOR_RATE, rate="<<_tasRate<<endl;
    ioctl(USB2D_SET_SOR_RATE, (void *) &_tasRate, sizeof (int));

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}

void TwoD_USB::close() throw(n_u::IOException)
{
    if (DerivedDataReader::getInstance())
	    DerivedDataReader::getInstance()->removeClient(this);
    DSMSensor::close();
}


/*---------------------------------------------------------------------------*/
#include <cstdio>
bool TwoD_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    return true;
}

void TwoD_USB::addSampleTag(SampleTag * tag)
throw(n_u::InvalidParameterException)
{
    DSMSensor::addSampleTag(tag);
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::fromDOMElement(const xercesc::DOMElement * node)
throw(n_u::InvalidParameterException)
{
    DSMSensor::fromDOMElement(node);

    const Parameter *p;

    // Acquire probe diode/pixel resolution (in micrometers) for tas encoding.
    p = getParameter("RESOLUTION");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "RESOLUTION","not found");
    _resolution = p->getNumericValue(0) * 1.0e-6;

    p = getParameter("TAS_RATE");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "TAS_RATE","not found");
    setTASRate((int)(rint(p->getNumericValue(0)))); //tas_rate is the same rate used as sor_rate
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::derivedDataNotify(const nidas::core::DerivedDataReader *
                                  s) throw()
{
    // std::cerr << "tas " << s->getTrueAirspeed() << std::endl;
    if (!::isnan(s->getTrueAirspeed())) {
	try {
	    sendTrueAirspeed(s->getTrueAirspeed());
	}
	catch(const n_u::IOException & e)
	{
	    n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
	}
    }
}

/*---------------------------------------------------------------------------*/
int TwoD_USB::TASToTap2D(Tap2D * t2d, float tas, float resolution)
{
        double freq = tas / resolution;
	double minfreq;

	memset(t2d, 0, sizeof(*t2d));

	/*
	 * Minimum frequency we can generate is either:
	 *
	 *   1 MHz (with no frequency divider)
	 *      OR
	 *   100 kHz (using frequency divider factor 10)
	 */
	if (freq >= 1.0e6) {
	  t2d->div10 = 0;
	  minfreq = 1.0e6;
	}
	else if (freq >= 1.0e5) {
	  t2d->div10 = 1;  // set the divide-by-ten flag
	  minfreq = 1.0e5;
	}
	else {
	  /*
	   * Desired frequency is too low.  Fill the struct to generate 
	   * the lowest possible frequency and return -EINVAL to let the 
	   * caller know that the TAS is too low.
	   */
	  t2d->ntap = 0;
	  t2d->div10 = 1;
	  return -EINVAL;
	}
	  
        t2d->ntap = (unsigned char) ((1 - (minfreq / freq)) * 255);

        return 0;               /* Return success */
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::sendTrueAirspeed(float tas) throw(n_u::IOException)
{
    Tap2D tx_tas;
    if (TASToTap2D(&tx_tas, tas, _resolution)) 
	n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: TASToTap2D reports bad airspeed=%f m/s\n",
		getName().c_str(),tas);
	
    ioctl(USB2D_SET_TAS, (void *) &tx_tas, sizeof (Tap2D));
}
void TwoD_USB::printStatus(std::ostream& ostr) throw()
{

    DSMSensor::printStatus(ostr);
    struct usb_twod_stats status;

    try {
	ioctl(USB2D_GET_STATUS,&status,sizeof(status));
	long long tnow = getSystemTime();
	float imagePerSec = float(status.numImages - _numImages) /
		float(tnow - _lastStatusTime) * USECS_PER_SEC;
	_numImages = status.numImages;
	_lastStatusTime = tnow;

	ostr << "<td align=left>" << "imgBlks/sec=" <<
		fixed << setprecision(1) << imagePerSec <<
		",lost=" << status.lostImages << ",lostSOR=" << status.lostSORs <<
		",lostTAS=" << status.lostTASs << ", urbErrs=" << status.urbErrors <<
		"</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
}
