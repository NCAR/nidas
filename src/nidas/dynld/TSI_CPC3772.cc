// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/dynld/TSI_CPC3772.h>
#include <nidas/core/AsciiSscanf.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(TSI_CPC3772)

void TSI_CPC3772::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);
    if (getSampleTags().size() > 1)
        throw n_u::InvalidParameterException(getName(),
            "addSampleTag","does not support more than 1 sample tag");

    if (stag->getRate() > 0.0) {
        _deltaTusecs = (int)rint(USECS_PER_SEC / stag->getRate());
        _rate = (int)rint(stag->getRate());
    }

}

bool TSI_CPC3772::process(const Sample* samp,list<const Sample*>& results)
	throw()
{

    assert(samp->getType() == CHAR_ST);
    const char* inputstr = (const char*)samp->getConstVoidDataPtr();

    SampleT<float>* isamp = getSample<float>(getMaxScanfFields());

    const SampleTag* stag = 0;
    int nparsed = 0;
    const std::list<AsciiSscanf*>& xscanfers = getScanfers();
    std::list<AsciiSscanf*>::const_iterator si = xscanfers.begin();
    for ( ; si != xscanfers.end(); ++si) {
	AsciiSscanf* sscanf = *si;
	nparsed = sscanf->sscanf(inputstr,isamp->getDataPtr(),
		sscanf->getNumberOfFields());
	if (nparsed > 0) {
	    stag = sscanf->getSampleTag();
	    isamp->setId(stag->getId());
	    break;
	}
    }

    if (!nparsed) {
	isamp->freeReference();	// remember!
	return false;		// no sample
    }

    isamp->setTimeTag(samp->getTimeTag());
    isamp->setDataLength(nparsed);

    if ((signed)isamp->getDataLength() == _rate) {
        const float* ifp = (const float*) isamp->getConstVoidDataPtr();
        for (unsigned int i = isamp->getDataLength(); i-- > 0; ) {
            SampleT<float>* osamp = getSample<float>(1);
            osamp->setTimeTag(isamp->getTimeTag() - i * _deltaTusecs);
            osamp->setId(isamp->getId());
            *osamp->getDataPtr() = *ifp++;
            results.push_back(osamp);
        }
    }
    isamp->freeReference();     // done with it.
    return !results.empty();
}

