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

NIDAS_CREATOR_FUNCTION_NS(raf, TwoDC_USB)

const n_u::EndianConverter * TwoDC_USB::fromBig =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);

const long long TwoDC_USB::_syncWord = 0xAAAAAA0000000000LL;
const long long TwoDC_USB::_syncMask = 0xFFFFFF0000000000LL;


TwoDC_USB::TwoDC_USB():_sorRate(1)
{
    cerr << __PRETTY_FUNCTION__ << endl;

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

    cerr << __PRETTY_FUNCTION__ << "open-begin" << endl;

    // Shut the probe down until a valid TAS comes along.
    sendTrueAirspeed(33.0);

    cerr << "SET_SOR_RATE, rate="<<_sorRate<<endl;
    ioctl(USB2D_SET_SOR_RATE, (void *) &_sorRate, sizeof (int));

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);

    cerr << __PRETTY_FUNCTION__ << "open-end" << endl;
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
    int id = *lptr++;
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
    int id = *lptr++;
    long tas = *lptr++;
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
    int id = *lptr++;

    /* From the driver level, id=0 is image data, id=1 is SOR.
     * Note that this is different from the typical XML configuration
     * where for processed samples, id=sensorID+1 are the images,
     * and id=sensorID+2 are the SORs.
     */
    switch (id) {
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

    std::cerr << __PRETTY_FUNCTION__ << "fromDOMElement-end" << endl;
}

/*---------------------------------------------------------------------------*/
void TwoDC_USB::derivedDataNotify(const nidas::core::DerivedDataReader *
                                  s) throw()
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

    try {
        ioctl(USB2D_SET_TAS, (void *) &tx_tas, sizeof (Tap2D));
    }
    catch(const n_u::IOException & e)
    {
        n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
    }
}
