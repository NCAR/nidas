/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/Sample.h>

#include <iostream>

const float nidas::core::floatNAN = nanf("");

const float nidas::core::doubleNAN = nan("");

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

#ifdef MUTEX_PROTECT_REF_COUNTS
/* static */
// n_u::Mutex Sample::refLock;
#endif

/* static */
int Sample::_nsamps = 0;

Sample* nidas::core::getSample(sampleType type, unsigned int len)
{
    Sample* samp = 0;
    unsigned int lin = len;
    try {
        switch(type) {
        case CHAR_ST:
            samp = getSample<char>(len);
            break;
        case UCHAR_ST:
            samp = getSample<unsigned char>(len);
            break;
        case SHORT_ST:
            len /= sizeof(short);
            if (len * sizeof(short) != lin) return 0;
            samp = getSample<short>(len);
            break;
        case USHORT_ST:
            len /= sizeof(short);
            if (len * sizeof(short) != lin) return 0;
            samp = getSample<unsigned short>(len);
            break;
        case INT32_ST:
            assert(sizeof(int) == 4);
            len /= sizeof(int);
            if (len * sizeof(int) != lin) return 0;
            samp = getSample<int>(len);
            break;
        case UINT32_ST:
            assert(sizeof(unsigned int) == 4);
            len /= sizeof(unsigned int);
            if (len * sizeof(unsigned int) != lin) return 0;
            samp = getSample<unsigned int>(len);
            break;
        case FLOAT_ST:
            len /= sizeof(float);
            if (len * sizeof(float) != lin) return 0;
            samp = getSample<float>(len);
            break;
        case DOUBLE_ST:
            len /= sizeof(double);
            if (len * sizeof(double) != lin) return 0;
            samp = getSample<double>(len);
            break;
        case INT64_ST:
            len /= sizeof(long long);
            if (len * sizeof(long long) != lin) return 0;
            samp = getSample<long long>(len);
            break;
        case UNKNOWN_ST:
        default:
            return 0;
        }
    }
    catch (const SampleLengthException& e) {
        return 0;
    }
    return samp;
}
