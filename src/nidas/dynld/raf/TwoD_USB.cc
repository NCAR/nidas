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

const n_u::EndianConverter * TwoD_USB::bigEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);

TwoD_USB::TwoD_USB() : _tasRate(1),
    _sorID(0), _1dcID(0), _2dcID(0),
    _size_dist_1D(0), _size_dist_2D(0),
    _totalRecords(0),_totalParticles(0),
    _rejected1D_Cntr(0), _rejected2D_Cntr(0),
    _overLoadSliceCount(0), _overSizeCount_2D(0),
    _tasOutOfRange(0),_misAligned(0),_suspectSlices(0),
    _prevTime(0),_histoEndTime(0),
    _trueAirSpeed(floatNAN),
    _saveBuffer(0),_savedBytes(0),_savedAlloc(0)
{
}

TwoD_USB::~TwoD_USB()
{
    delete [] _size_dist_1D;
    delete [] _size_dist_2D;

    if (_totalRecords > 0) {
        std::cerr << "Total number of 2D records = " << _totalRecords << std::endl;
        std::cerr << "Total number of 2D particles detected = " << _totalParticles << std::endl;
        std::cerr << "Number of rejected particles for 1D = " << _rejected1D_Cntr << std::endl;
        std::cerr << "Number of rejected particles for 2D = " << _rejected2D_Cntr << std::endl;
        std::cerr << "Number of overload words = " << _overLoadSliceCount << std::endl;
        std::cerr << "2D over-sized particle count = " << _overSizeCount_2D << std::endl;
        std::cerr << "Number of times TAS out of range = " << _tasOutOfRange << std::endl;
        std::cerr << "Number of misaligned sync words = " << _misAligned << std::endl;
        std::cerr << "Number of suspect slices = " << _suspectSlices << std::endl;
    }
    delete [] _saveBuffer;
}

IODevice *TwoD_USB::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner *TwoD_USB::buildSampleScanner()
{   
    return new DriverSampleScanner((4104 + 8) * 4);
}


/*---------------------------------------------------------------------------*/
void TwoD_USB::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    DSMSensor::open(flags);
    init_parameters();

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
/* Initialization of things that are needed in real-time
 * and when post-processing.  Don't put stuff here that
 * is *only* needed during post-processing (the idea is to
 * save memory on DSMs).
 */
void TwoD_USB::init_parameters() throw(n_u::InvalidParameterException)
{
    const Parameter *p;

    // Acquire probe diode/pixel resolution (in micrometers) for tas encoding.
    p = getParameter("RESOLUTION");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "RESOLUTION","not found");
    _resolutionMicron = (int)p->getNumericValue(0);
    _resolutionMeters = (float)_resolutionMicron * 1.0e-6;
   
    p = getParameter("TAS_RATE");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "TAS_RATE","not found");
    setTASRate((int)(rint(p->getNumericValue(0)))); //tas_rate is the same rate used as sor_rate
}

/*---------------------------------------------------------------------------*/
/* Stuff that is necessary when post-processing.
 */
void TwoD_USB::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    init_parameters();

    // Find SampleID for 1D & 2D arrays.
    const list<const SampleTag *>& tags = getSampleTags();
    list<const SampleTag *>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag * tag = *si;
        Variable & var = ((SampleTag *)tag)->getVariable(0);

        if (var.getName().compare(0, 3, "A1D") == 0)
            _1dcID = tag->getId();
        if (var.getName().compare(0, 3, "A2D") == 0)
            _2dcID = tag->getId();
    }

    _prevTime = 0;

    _twoDAreaRejectRatio = 0.5;
    const Parameter * p = getParameter("AREA_RATIO_REJECT");
    if (p) {
        _twoDAreaRejectRatio = p->getNumericValue(0);
    }

    delete [] _size_dist_1D;
    delete [] _size_dist_2D;
    _size_dist_1D = new unsigned int[NumberOfDiodes()];
    _size_dist_2D = new unsigned int[NumberOfDiodes()<<1];
    clearData();
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
    // std::cerr << "tas " << s->getTrueAirspeed() << std::endl;
    _trueAirSpeed = s->getTrueAirspeed();   // save it to display in printStatus
    if (!::isnan(_trueAirSpeed)) {
	try {
	    sendTrueAirspeed(_trueAirSpeed);
	}
	catch(const n_u::IOException & e)
	{
	    n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
	}
    }
}

/*---------------------------------------------------------------------------*/
int TwoD_USB::TASToTap2D(Tap2D * t2d, float tas)
{
    /* Default tas to spinning disk speed if we are not moving.  This
     * will probably bite us some day when they try to use a 2D probe on
     * ISF or ISFF....
     */
    if (tas < 33.0)
        tas = 33.0;

    double freq = tas / getResolution();
    double minfreq;

    memset(t2d, 0, sizeof(*t2d));

    /*
     * Minimum frequency we can generate is either:
     *
     *   1 MHz (with no frequency divider)
     *      OR
     *   100 kHz (using frequency divider factor 10)
     *
     *   _resolution test forces 2DC to that freq.
     */
    if (_resolutionMicron == 25 || freq >= 1.0e6) {
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
    t2d->dummy = (unsigned char )tas;
    return 0;               /* Return success */
}

/*---------------------------------------------------------------------------*/
float TwoD_USB::Tap2DToTAS(const Tap2D * t2d) const
{
    float tas = (1.0e6 / (1.0 - ((float)t2d->ntap / 255))) * getResolution();

    if (t2d->div10 == 1)
        tas /= 10.0;

    return tas;
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::sendTrueAirspeed(float tas) throw(n_u::IOException)
{
    Tap2D tx_tas;
    if (TASToTap2D(&tx_tas, tas)) 
	n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: TASToTap2D reports bad airspeed=%f m/s\n",
		getName().c_str(),tas);
	
    ioctl(USB2D_SET_TAS, (void *) &tx_tas, sizeof (Tap2D));
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }
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
                ",TAS=" << setprecision(0) << _trueAirSpeed << "m/s" <<
		"</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::createSamples(dsm_time_t nextTimeTag,list < const Sample * >&results)
    throw()
{
    int nvalues;
    SampleT < float >*outs;
    float * dout;

    if (nextTimeTag < _histoEndTime) return;
    if (_histoEndTime == 0) {
        _histoEndTime = nextTimeTag + USECS_PER_SEC - (int)(nextTimeTag % USECS_PER_SEC);
        return;
    }

    if (_1dcID != 0) {

        // Sample 2 is the 1D enter-in data.
        nvalues = NumberOfDiodes() + 1;
        outs = getSample < float >(nvalues);

        // time tag is the start of the histogram
        outs->setTimeTag(_histoEndTime - USECS_PER_SEC);
        outs->setId(_1dcID);

        dout = outs->getDataPtr();
        for (int i = 0; i < NumberOfDiodes(); ++i)
            *dout++ = (float)_size_dist_1D[i];

        *dout++ = _dead_time_1D / 1000;      // Dead Time, return milliseconds.
        results.push_back(outs);
    }

    // Sample 3 is the 2D center-in or reconstruction data.
    if (_2dcID != 0) {
        nvalues = (NumberOfDiodes()<<1) + 1;
        outs = getSample < float >(nvalues);

        // time tag is the start of the histogram
        outs->setTimeTag(_histoEndTime - USECS_PER_SEC);
        outs->setId(_2dcID);

        dout = outs->getDataPtr();
        for (int i = 0; i < (NumberOfDiodes()<<1); ++i)
            *dout++ = (float)_size_dist_2D[i];

        *dout++ = _dead_time_2D / 1000;      // Dead Time, return milliseconds.
        results.push_back(outs);
    }

    clearData();

    // end time of next histogram
    _histoEndTime += USECS_PER_SEC;
    if (_histoEndTime <= nextTimeTag)
        _histoEndTime = nextTimeTag + USECS_PER_SEC - (int)(nextTimeTag % USECS_PER_SEC);
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::processParticleSlice(Particle& p, const unsigned char * data)
{

    int nBytes = NumberOfDiodes() / 8;

    /* Note that 2D data is inverted.  So a '1' means no shadowing of the diode.
     * '0' means shadowing and a particle.  Perform complement here.
     */
    unsigned char slice[nBytes];
    for (int i = 0; i < nBytes; ++i)
        slice[i] = ~(data[i]);

    p.width++;

    if ((slice[0] & 0x80)) { // touched edge
        p.edgeTouch |= 0x0F;
    }

    if ((slice[nBytes-1] & 0x01)) { // touched edge
        p.edgeTouch |= 0xF0;
    }

    // Compute area = number of bits set in particle
    for (int i = 0; i < nBytes; ++i)
    {
        unsigned char c = slice[i];
        for (; c; p.area++)
            c &= c - 1; // clear the least significant bit set
    }

    // number of bits between first and last set bit, inclusive
    int h = NumberOfDiodes();
    for (int i = 0; i < nBytes; ++i)
    {
        if (slice[i] == 0)
        {
            h -= 8;
            continue;
        }
        int r = 7;
        unsigned char v = slice[i];
        while (v >>= 1)
            r--;
        h -= r;
        break;
    }
    for (int i = nBytes-1; i >= 0; --i)
    {
        if (slice[i] == 0)
        {
            h -= 8;
            continue;
        }
        int r = 0;
        unsigned char v = slice[i];
        while ((v & 0x01) == 0)
        {
            r++;
            v >>= 1;
        }
        h -= r;
        break;
    }

    if (h > 0)
        p.height = std::max((unsigned)h, p.height);
}

/*---------------------------------------------------------------------------*/
bool TwoD_USB::acceptThisParticle1D(const Particle& p) const
{
    if (!p.edgeTouch && p.height > 0 && p.height * 4 >= p.width)
        return true;

    return false;
}

bool TwoD_USB::acceptThisParticle2D(const Particle& p) const
{
    if (p.width > 121 ||
       (p.height < 24 && p.width > 6 * p.height) ||
       (p.height < 6 && p.width > 3 * p.height) ||
       (p.edgeTouch && (float)p.height / p.width < 0.2))
        return false;

    if ((float)p.area / (p.width * p.height) <= _twoDAreaRejectRatio)
        return false;

    if (p.edgeTouch && p.width > p.height * 2)	// Center-in
        return false;

    return true;
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::countParticle(const Particle& p, float resolutionUsec)
{
    // 1D
    if (acceptThisParticle1D(p))
        _size_dist_1D[p.height]++;
    else {
        float liveTime = resolutionUsec * p.width;
        _dead_time_1D += liveTime;
        _rejected1D_Cntr++;
    }

    // 2D - Center-in algo
    if (acceptThisParticle2D(p)) {
        int n = std::max(p.height, p.width);
        if (n < (NumberOfDiodes()<<1))
            _size_dist_2D[n]++;
        else
            _overSizeCount_2D++;
    }
    else {
        float liveTime = resolutionUsec * p.width;
        _dead_time_2D += liveTime;
        _rejected2D_Cntr++;
    }
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::clearData()
{
    ::memset(_size_dist_1D, 0, NumberOfDiodes()*sizeof(unsigned int));
    ::memset(_size_dist_2D, 0, NumberOfDiodes()*sizeof(unsigned int)*2);

    _dead_time_1D = _dead_time_2D = 0.0;
}

void TwoD_USB::setupBuffer(const unsigned char** cp,const unsigned char** eod)
{
    if (_savedBytes > 0) {
        int lrec = *eod - *cp;
        int l = _savedBytes + lrec;
        if (_savedAlloc < l) {
            unsigned char* newBuffer = new unsigned char[l];
            if (_savedBytes > 0)
                ::memcpy(newBuffer,_saveBuffer,_savedBytes);
            delete [] _saveBuffer;
            _saveBuffer = newBuffer;
            _savedAlloc = l;
        }
        ::memcpy(_saveBuffer+_savedBytes,*cp,lrec);
        *cp = _saveBuffer;
        *eod = _saveBuffer + _savedBytes + lrec;
    }
}
void TwoD_USB::saveBuffer(const unsigned char* cp, const unsigned char* eod)
{
    assert(eod >= cp);
    int lrec = eod - cp;
#ifdef DEBUG
    if (lrec > 0) cerr << "saving " << lrec << endl;
#endif
    if (_savedAlloc < lrec) {
        // cp may point into _saveBuffer
        unsigned char* newBuffer =  new unsigned char[lrec];
        ::memcpy(newBuffer,cp,lrec);
        delete [] _saveBuffer;
        _saveBuffer = newBuffer;
        _savedAlloc = lrec;
    }
    else if (lrec > 0) ::memmove(_saveBuffer,cp,lrec);
    _savedBytes = lrec;
}

