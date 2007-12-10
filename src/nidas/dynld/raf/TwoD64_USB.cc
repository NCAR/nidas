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

const long long TwoD64_USB::_syncWord = 0xAAAAAA0000000000LL;
const long long TwoD64_USB::_syncMask = 0xFFFFFF0000000000LL;


TwoD64_USB::TwoD64_USB()
{
    _syncWordBE = bigEndian->longlongValue(&_syncWord);
    _syncMaskBE = bigEndian->longlongValue(&_syncMask);
}

TwoD64_USB::~TwoD64_USB()
{
}

void TwoD64_USB::fromDOMElement(const xercesc::DOMElement * node)
throw(n_u::InvalidParameterException)
{
    TwoD_USB::fromDOMElement(node);

    /* Look for a sample tag with id=2. This is assumed to be
     * the shadowOR sample.  Check its rate.
     */
    float sorRate = 0.0;
    const list<const SampleTag*>& tags = getSampleTags();
    list<const SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag* tag = *si;
        if (tag->getId() - getId() == 2) 
		sorRate = tag->getRate();
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
    
    unsigned long lin = samp->getDataByteLength();

    if (lin < 2 * sizeof (long))
        return false;

    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    /*int stype =*/ bigEndian->longValue(*lptr++);
    long sor = bigEndian->longValue(*lptr++);

    size_t nvalues = 1;
    SampleT < float >*outs = getSample < float >(nvalues);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 2);   //
    float *dout = outs->getDataPtr();
    *dout = sor;
    results.push_back(outs);
    return true;
}

bool TwoD64_USB::processImage(const Sample * samp,
                             list < const Sample * >&results) throw()
{

    unsigned long lin = samp->getDataByteLength();
    if (lin < 2 * sizeof (long) + 512 * sizeof (long long))
        return false;
    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    /*int stype =*/ bigEndian->longValue(*lptr++);
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
        if ((*llptr & _syncMaskBE) == _syncWordBE) cnt++;
    }

    *dout = cnt;
    results.push_back(outs);

    return true;
}

/*---------------------------------------------------------------------------*/
#include <cstdio>
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
        return processImage(samp, results);
    case 1:
        return processSOR(samp, results);
    }
    return false;
}



