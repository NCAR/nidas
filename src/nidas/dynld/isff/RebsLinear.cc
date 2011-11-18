// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/RebsLinear.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,RebsLinear)

RebsLinear::RebsLinear(): Polynomial()
{
    float tmpcoefs[] = { 0.0, 1.0, 0.0, 1.0 };
    setCoefficients(tmpcoefs,sizeof(tmpcoefs)/sizeof(tmpcoefs[0]));
}

RebsLinear* RebsLinear::clone() const
{
    return new RebsLinear(*this);
}

string RebsLinear::toString()
{
    const std::vector<float>& coefs = getCoefficients();

    ostringstream ost;
    ost << "rebslinear ";
    if (getCalFile()) 
        ost << "calfile=" << getCalFile()->getPath() << ' ' << getCalFile()->getFile() << endl;
    else {
        ost << "coefs=";
        for (unsigned int i = 0; i < NUM_COEFS; i++) {
            if (i < coefs.size()) ost << coefs[i] << ' ';
            else ost << coefs[i] << "nan";
        }
    }
    ost << " units=\"" << getUnits() << "\"" << endl;
    return ost.str();
}

float RebsLinear::convert(dsm_time_t t,float val)
{
    readCalFile(t);

    int n;
    const float* coefs = getCoefficients(n);

    if (val < 0) {
        if (n > SLOPE_NEG)
            return val * coefs[SLOPE_NEG] + coefs[INTCP_NEG];
    }
    else if (n > SLOPE_POS)
            return val * coefs[SLOPE_POS] + coefs[INTCP_POS];
    return floatNAN;
}

