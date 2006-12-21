/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-09-10 07:54:23 -0600 (Sat, 10 Sep 2005) $

    $LastChangedRevision: 2879 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/CSAT3_Sonic.h $

*/

#ifndef NIDAS_DYNLD_ISFF_REBSLINEAR_H
#define NIDAS_DYNLD_ISFF_REBSLINEAR_H

#include <nidas/core/VariableConverter.h>

#include <cmath>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * A linear converter useful for REBS net radiometers, which
 * have two sets of linear coefficients, one set to be applied
 * during the day when Rnet > 0 and one when Rnet < 0: i.e.
 * a linear conversion with an if-test (wow! highly complicated code).
 * Coefficients are read from a calibration file in this order
 *  intercept_neg   slope_neg   intercept_pos   slope_pos
 */
class RebsLinear: public nidas::core::Polynomial
{
public:

    RebsLinear();

    RebsLinear* clone() const;

    void setCoefficients(const std::vector<float>& vals);

    std::string toString();

    /**
     * Apply linear conversion.
     */
    float convert(dsm_time_t t, float volts);

protected:

    /** Order of coefficients */
    enum coef { INTCP_NEG, SLOPE_NEG, INTCP_POS, SLOPE_POS, NUM_COEFS };

    /** 2 sets of linear coefficients */
    float coefs[NUM_COEFS];

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
