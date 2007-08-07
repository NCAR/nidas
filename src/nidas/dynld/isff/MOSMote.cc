/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/MOSMote.h>

#include <sstream>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,MOSMote)

bool MOSMote::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{
    unsigned int nc = samp->getDataByteLength();
    if (nc == 0) return false;

    SampleT<char>* nsamp = getSample<char>(nc);

    nsamp->setTimeTag(samp->getTimeTag());
    nsamp->setId(samp->getId());

    const char *cp = (const char*) samp->getConstVoidDataPtr();
    const char *ep = cp + nc;
    char *op = nsamp->getDataPtr();

    for ( ; cp < ep; cp++ ) if (*cp != '\0') *op++ = *cp;
    *op = 0;
    nsamp->setDataLength(op - (char*)nsamp->getDataPtr());

    bool res = DSMSerialSensor::process(nsamp,results);
    nsamp->freeReference();
    return res;
}
