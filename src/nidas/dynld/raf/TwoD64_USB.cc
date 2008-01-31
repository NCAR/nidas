/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD64_USB.cc $

 ******************************************************************
*/


#include <nidas/linux/usbtwod/usbtwod.h>
#include <nidas/dynld/raf/TwoD64_USB.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD64_USB)

#ifdef THE_KNIGHTS_WHO_SAY_NI
//  I'm leaving these three in for documentation.
const unsigned long long TwoD64_USB::_syncWord = 0xAAAAAA0000000000LL;
const unsigned long long TwoD64_USB::_syncMask = 0xFFFFFF0000000000LL;
const unsigned long long TwoD64_USB::_overldWord = 0x5555AA0000000000LL;
#endif

const unsigned char TwoD64_USB::_syncString[3] = { 0xAA, 0xAA, 0xAA };
const unsigned char TwoD64_USB::_overldString[3] = { 0x55, 0x55, 0xAA };

TwoD64_USB::TwoD64_USB()
{
    _size_dist_1DC = new size_t[NumberOfDiodes()];
    _size_dist_2DC = new size_t[NumberOfDiodes()<<1];
    clearData();
}

TwoD64_USB::~TwoD64_USB()
{
    delete [] _size_dist_1DC;
    delete [] _size_dist_2DC;
}

void TwoD64_USB::fromDOMElement(const xercesc::DOMElement * node)
throw(n_u::InvalidParameterException)
{
    TwoD_USB::fromDOMElement(node);

    /* Look for a sample tag with id=2. This is assumed to be
     * the shadowOR sample.  Check its rate.
     */
    float sorRate = 0.0;
    const list<const SampleTag *>& tags = getSampleTags();
    list<const SampleTag *>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag * tag = *si;
        Variable & var = ((SampleTag *)tag)->getVariable(0);

        if (var.getName().compare(0, 6, "SHDORC") == 0) {
            sorRate = tag->getRate();
            _sorID = tag->getId();
        }
    }
    if (sorRate <= 0.0) throw n_u::InvalidParameterException(getName(),
	"sample","shadow OR sample rate not found");
    if (sorRate != getTASRate()) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: shadowOR sample rate=%f is not equal to TAS_RATE=%f, continuing\n",
		getName().c_str(),sorRate,getTASRate());
    }
}

bool TwoD64_USB::processSOR(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    unsigned long len = samp->getDataByteLength();
    if (len < 2 * sizeof (long))
        return false;

    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();

    /*int stype =*/ bigEndian->longValue(*lptr++);
    long sor = bigEndian->longValue(*lptr++);

    size_t nvalues = 1;
    SampleT < float >*outs = getSample < float >(nvalues);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(_sorID);
    float *dout = outs->getDataPtr();
    *dout = sor;

    results.push_back(outs);
    return true;
}

bool TwoD64_USB::processImageRecord(const Sample * samp,
                             list < const Sample * >&results) throw()
{
    bool rc = false;	// return code.

    unsigned long len = samp->getDataByteLength();

    if (len < 2 * sizeof (long) + 512 * sizeof (long long))
        return rc;

    unsigned long long startTime = _prevTime;
    unsigned long long endTime = samp->getTimeTag();
    _prevTime = endTime;
    _totalRecords++;

    if (_nowTime == 0)
    {
        _nowTime = samp->getTimeTag();
        _nowTime -= (_nowTime % USECS_PER_SEC);	// nowTime should have no fractional component.
        return rc;	// Chuck first record as we don't know start time.
    }

    const int * dp = (const int *) samp->getConstVoidDataPtr();
    dp++; // Move past sample type.

    float tas = Tap2DToTAS((Tap2D *)dp++);
    if (tas < 0.0 || tas > 300.0) throw n_u::InvalidParameterException(getName(),
        "TAS","out of range");

    float frequency = getResolutionMicron() / tas;


    // Loop through all slices in record.
    unsigned long long * p = (unsigned long long *)dp;
    unsigned long long	firstTimeWord = 0;	// First timing word in this record.
    for (size_t i = 0; i < 512; ++i, ++p)
    {
        if (_cp == 0)
            _cp = new Particle;

        /* Four cases, syncWord, overloadWord, blank or legitimate slice.
         * sync & overload words come at the end of the particle.
         */

        // Typical time/sync word, terminates particle.
        if (::memcmp(p, _syncString, 3) == 0) {
            _totalParticles++;

            // time words are from a 12MHz clock
            unsigned long long slice = bigEndian->longlongValue(*p);
            unsigned long long thisTimeWord = (slice & 0x000000ffffffffffLL) / 12;

            if (firstTimeWord == 0)
                firstTimeWord = thisTimeWord;

            // Approx millisecondes since start of record.
            unsigned long long tBarElapsedtime = thisTimeWord - firstTimeWord;
            unsigned long long thisParticleSecond = startTime - tBarElapsedtime;
            thisParticleSecond -= (thisParticleSecond % USECS_PER_SEC);

            // If we have crossed the 1 second boundary, send existing data and reset.
            if (thisParticleSecond != _nowTime)
            {
                sendData(_nowTime, results);
                _nowTime = thisParticleSecond;
                rc = true;
            }

            countParticle(_cp, frequency);
            delete _cp; _cp = 0;
        }
        else
        // overload word, reject this particle.
        if (::memcmp(p, _overldString, 3) == 0) {
            _overLoadSliceCount++;
            delete _cp; _cp = 0;
        }
        // Blank slice.  If blank is mid particle, then reject.
        if (*p == 0xffffffffffffffffLL) {
            if (i == 511 || ::memcmp(&p[1], _syncString, 3))
                delete _cp; _cp = 0;
        }
        else {
            processParticleSlice(_cp, (const unsigned char *)p);
        }
    }


    _nowTime = samp->getTimeTag();
    _nowTime -= (_nowTime % USECS_PER_SEC);	// to seconds
    return rc;
}

bool TwoD64_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    assert(sizeof (long long) == 8);

    if (samp->getDataByteLength() < sizeof (long))
        return false;

    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    int stype = bigEndian->longValue(*lptr++);

    /* From the usbtwod driver: stype=0 is image data, stype=1 is SOR.  */
    switch (stype) {
    case 0:                    // image data
        return processImageRecord(samp, results);
    case 1:
        return processSOR(samp, results);
    }
    return false;
}
