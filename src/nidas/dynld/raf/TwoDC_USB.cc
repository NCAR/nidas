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
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoDC_USB)

const n_u::EndianConverter * TwoDC_USB::fromBig =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);

const long long TwoDC_USB::_syncWord = 0xAAAAAA0000000000LL;
const long long TwoDC_USB::_syncMask = 0xFFFFFF0000000000LL;


TwoDC_USB::TwoDC_USB():_sorRate(1)
{
}

TwoDC_USB::~TwoDC_USB()
{
}

IODevice *TwoDC_USB::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner *TwoDC_USB::buildSampleScanner()
{   
    return new SampleScanner((4104 + 8) * 4);
}


/*---------------------------------------------------------------------------*/
void TwoDC_USB::open(int flags) throw(n_u::IOException)
{
    DSMSensor::open(flags);

    // Shut the probe down until a valid TAS comes along.
    sendTrueAirspeed(33.0);

    cerr << "SET_SOR_RATE, rate="<<_sorRate<<endl;
    ioctl(USB2D_SET_SOR_RATE, (void *) &_sorRate, sizeof (int));

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
}

void TwoDC_USB::close() throw(n_u::IOException)
{
    DerivedDataReader::getInstance()->removeClient(this);
    DSMSensor::close();
}

bool TwoDC_USB::processSOR(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    
    unsigned long lin = samp->getDataByteLength();

    if (lin < 2 * sizeof (long))
        return false;

    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    int stype = fromBig->longValue(*lptr++);
    long sor = fromBig->longValue(*lptr++);

    size_t nvalues = 1;
    SampleT < float >*outs = getSample < float >(nvalues);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 2);   //
    float *dout = outs->getDataPtr();
    *dout = sor;
    results.push_back(outs);
    return true;
}

bool TwoDC_USB::processImage(const Sample * samp,
                             list < const Sample * >&results) throw()
{

    unsigned long lin = samp->getDataByteLength();
    if (lin < 2 * sizeof (long) + 512 * sizeof (long long))
        return false;
    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    int stype = fromBig->longValue(*lptr++);
    *lptr++;		// skip 4 byte TAS structure
    const long long *llptr = (const long long *) lptr;

    // We will compute 1 value, a count of particles.
    size_t nvalues = 1;
    SampleT < float >*outs = getSample < float >(nvalues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);   //

    float *dout = outs->getDataPtr();

    // Count number of particles (sync words) in the record and return.
    int cnt = 0;
    for (int i = 0; i < 512; ++i, llptr++) {
        if (memcmp(llptr, ((const char *) &_syncWord) + 5, 3) == 0)
            ++cnt;
    }

    *dout = cnt;
    results.push_back(outs);

    return true;
}

/*---------------------------------------------------------------------------*/
#include <cstdio>
bool TwoDC_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    assert(sizeof (long long) == 8);

    if (samp->getDataByteLength() < sizeof (long))
        return false;

    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    int stype = fromBig->longValue(*lptr++);

    /* From the usbtwod driver: stype=0 is image data, stype=1 is SOR.  */
    switch (stype) {
    case 0:                    // image data
        return processImage(samp, results);
    case 1:
        return processSOR(samp, results);
    }
    return false;
}

void TwoDC_USB::addSampleTag(SampleTag * tag)
throw(n_u::InvalidParameterException)
{
    DSMSensor::addSampleTag(tag);
    // To get the basic sample id (e.g. 1, 2, etc)
    // subtract the sensor id
    if (tag->getId() - getId() == 2)
        _sorRate = (int) rint(tag->getRate());
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
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::derivedDataNotify(const nidas::core::DerivedDataReader *
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

int TwoDC_USB::TASToTap2D(Tap2D * t2d, float tas, float resolution)
{
        double freq = tas / resolution;
        unsigned int ntap = (unsigned int) ((1 - (1.0e6 / freq)) * 255);
	memset(t2d,0,sizeof(Tap2D));

        t2d->vdiv = 0;          /* currently unused */
        t2d->cntr = 0;		/* counter, initialize to 0 */
        t2d->ntap = 0;

        if (ntap > 255)
                return -EINVAL;

        t2d->ntap = (unsigned char) ntap;
        return 0;               /* Return success */
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::sendTrueAirspeed(float tas) throw(n_u::IOException)
{
    Tap2D tx_tas;
    if (TASToTap2D(&tx_tas, tas, _resolution)) 
	n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: TASToTap2D reports bad airspeed=%f m/s\n",
		getName().c_str(),tas);
	
    ioctl(USB2D_SET_TAS, (void *) &tx_tas, sizeof (Tap2D));
}
void TwoDC_USB::printStatus(std::ostream& ostr) throw()
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
