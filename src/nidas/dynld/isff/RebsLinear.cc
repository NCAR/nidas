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
    setCoefficients(vector<float>(tmpcoefs,
        tmpcoefs+sizeof(tmpcoefs)/sizeof(tmpcoefs[0])));
}

RebsLinear* RebsLinear::clone() const
{
    return new RebsLinear(*this);
}

void RebsLinear::setCoefficients(const vector<float>& vals)
{
    Polynomial::setCoefficients(vals);
    for (unsigned int i = 0; i < NUM_COEFS && i < vals.size(); i++)
        coefs[i] = vals[i];
}

string RebsLinear::toString()
{
    ostringstream ost;
    ost << "rebslinear ";
    if (getCalFile()) 
        ost << "calfile=" << getCalFile()->getPath() << ' ' << getCalFile()->getFile() << endl;
    else {
        ost << "coefs=";
        for (unsigned int i = 0; i < NUM_COEFS; i++)
            ost << coefs[i] << ' ';
    }
    ost << " units=\"" << getUnits() << "\"" << endl;
    return ost.str();
}

float RebsLinear::convert(dsm_time_t t,float val)
{
    if (getCalFile()) {
        while(t >= calTime) {
            try {
                int n = getCalFile()->readData(coefs,NUM_COEFS);
                for (int i = n; i < NUM_COEFS; i++) coefs[i] = floatNAN;
                Polynomial::setCoefficients(vector<float>
                    (coefs,coefs+NUM_COEFS));
                calTime = getCalFile()->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    getCalFile()->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < NUM_COEFS; i++) coefs[i] = floatNAN;
                Polynomial::setCoefficients(vector<float>
                    (coefs,coefs+NUM_COEFS));
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    getCalFile()->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < NUM_COEFS; i++) coefs[i] = floatNAN;
                Polynomial::setCoefficients(vector<float>
                    (coefs,coefs+NUM_COEFS));
                calTime = LONG_LONG_MAX;
            }
        }
    }
    if (val < 0) return val * coefs[SLOPE_NEG] + coefs[INTCP_NEG];
    else return val * coefs[SLOPE_POS] + coefs[INTCP_POS];
}

