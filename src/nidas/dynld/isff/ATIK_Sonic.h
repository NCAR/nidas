// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2011-11-16 15:03:17 -0700 (Wed, 16 Nov 2011) $

    $LastChangedRevision: 6326 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/ATIK_Sonic.h $

*/

#ifndef NIDAS_DYNLD_ISFF_ATIK_SONIC_H
#define NIDAS_DYNLD_ISFF_ATIK_SONIC_H

#include <nidas/dynld/isff/SonicAnemometer.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * A class for making sense of data from an ATIK 3D sonic anemometer.
 */
class ATIK_Sonic: public SonicAnemometer
{
public:

    ATIK_Sonic();

    ~ATIK_Sonic();

    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    void validate()
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void pathShadowCorrection(float* uvwt);

    /**
     * Conversion factor from speed of sound squared to Kelvin.
     * When it reports temperature, the ATI sonic uses conversion
     * factor computed from a linear function of RH. The slope and
     * intercept of that function can be set by the user, as
     * well as a fixed value for RH:
     *      GAMMA_R = Gamma_ZeroRH + Gamma_Slope * RH_Value
     * The usual defaults are:
     *      Gamma_ZeroRH=402.434, Gamma_Slope=0.0404
     *      RH_Value = 20
     * which results in a value of
     *      Gamma_R = 403.242
     *
     * Note that the documenation for the CSAT3 suggest a value of:
     *      GAMMA_R = Gamma_d * Rd = 401.856
     * for dry air.
     */
    static const float GAMMA_R = 403.242;

private:

    /**
     * Requested number of output wind variables.
     */
    int _windNumOut;

    /**
     * Index in output sample of ldiag value.
     */
    int _ldiagIndex;

    /**
     * If user requests wind speed, variable name "spd", its index in the output sample.
     */
    int _spdIndex;

    /**
     * If user requests wind direction, variable name "dir", its index in the output sample.
     */
    int _dirIndex;

    /**
     * If user requests despike variables, e.g. "uflag","vflag","wflag","tcflag",
     * the index of "uflag" in the output variables.
     */
    int _spikeIndex;

    /**
     * If user requests output of counts values from sonic.
     */
    int _cntsIndex;

    /**
     * Output sample id
     */
    dsm_sample_id_t _sampleId;

    /**
     * Index transform vector for wind components.
     * Used for unusual sonic orientations, as when the sonic
     * is hanging down, when the usual sonic w axis becomes the
     * new u axis, u becomes w and v becomes -v.
     */
    int _tx[3];

    /**
     * Wind component sign conversion. Also used for unusual sonic
     * orientations, as when the sonic is hanging down, and the sign
     * of v is flipped.
     */
    int _sx[3];

    /**
     * The sonic can output the number of high-rate samples that
     * passed its internal checks. This is the expected number
     * of counts if all samples pass, typically 10 or 20.
     */
    int _expectedCounts;

    /**
     * If the fraction of missing counts is above this value,
     * then all output wind values are flagged.
     */
    float _diagThreshold;

    float _shadowFactor;
    float _maxShadowAngle;
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
