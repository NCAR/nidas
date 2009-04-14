/*
 ******************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: dongl $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/PPT_Serial.cc $

 ******************************************************************
*/

#include <nidas/dynld/raf/PPT_Serial.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PPT_Serial)

PPT_Serial::PPT_Serial()
{
}

PPT_Serial::~PPT_Serial()
{
}


void PPT_Serial::open(int flags) throw(n_u::IOException)
{
    DSMSerialSensor::open(flags);
}


void PPT_Serial::close() throw(n_u::IOException)
{
    DSMSerialSensor::close();
}


bool PPT_Serial::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    unsigned int nc = samp->getDataByteLength();
    if (nc == 0) return false;

    SampleT<char>* nsamp = getSample<char>(nc);

    nsamp->setTimeTag(samp->getTimeTag());
    nsamp->setId(samp->getId());

    const char *cp = (const char*) samp->getConstVoidDataPtr();
    const char *ep = cp + nc;
    char *op = nsamp->getDataPtr();
    for ( ; cp < ep; cp++) *op++ = *cp;
    *op++ = 0;

    // Fix in place, incorrectly formated negative numbers
    // by skipping the spaces between the '-' and the digits.
    cp = nsamp->getDataPtr();
    op = nsamp->getDataPtr();
    for ( ; *cp; op++ ) {
        if (op != cp) *op = *cp;
        if (*cp++ == '-') for ( ; *cp == ' '; cp++);
    }
    *op++ = 0;
    // cerr << nsamp->getDataPtr() << endl;
    nsamp->setDataLength(op - (char*)nsamp->getDataPtr());

    bool rc = DSMSerialSensor::process(nsamp, results);
    nsamp->freeReference();

    return rc;
}
