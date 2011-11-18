// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_REBSLINEAR_H
#define NIDAS_DYNLD_ISFF_REBSLINEAR_H

#include <nidas/core/VariableConverter.h>

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

    std::string toString();

    /**
     * Apply linear conversion.
     */
    float convert(dsm_time_t t, float volts);

private:

    /** Order of coefficients */
    enum coef { INTCP_NEG, SLOPE_NEG, INTCP_POS, SLOPE_POS, NUM_COEFS };

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
