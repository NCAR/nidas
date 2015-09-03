// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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

    void validate()
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    /**
     * Apply the path shadow correction and described in the comments
     * for _maxShadowAngle, and _shadowFactor.
     */
    void pathShadowCorrection(float* uvwt);

    /**
     * Placeholder for a method to remove a shadow correction that 
     * had been applied by the sonic firmware. This is so that
     * the user can choose to apply their own. This has not
     * been implemented yet, and will need parameters indicating
     * what factor and angle were applied by the sonic.
     */
    void removeShadowCorrection(float* );

    /**
     * Conversion factor from speed of sound squared to Kelvin.
     * When it reports temperature, the ATI sonic uses a conversion
     * factor computed from a linear function of RH. The slope and
     * intercept of that function as well as a fixed value for RH
     * can be set by the user in EEPROM on the sonic.
     * In computing sonic virtual temperature the sonic will then use:
     *      Tc = c^2 / GAMMA_R - 273.15
     *  Where
     *      GAMMA_R = Gamma_ZeroRH + Gamma_Slope * RH_Value
     * The usual defaults are:
     *      Gamma_ZeroRH=402.434, Gamma_Slope=0.0404
     *      RH_Value = 20
     * which results in a value of
     *      Gamma_R = 403.242
     *
     * This value of GAMMA_R is used to re-compute speed of sound from
     * temperature. The speed of sound, c, is then corrected for the 
     * curvature of the path between the transducers, using corrected
     * values for the wind components, and then the virtual temperature
     * re-computed from c.
     *
     * Note that the documenation for the CSAT3 suggest a value of:
     *      GAMMA_R = Gamma_d * Rd = 401.856
     * for dry air.
     *
     * These two values result in a difference of 1 degC for a typical
     * speed of sound of 340 m/s.
     * This may need to be resolved, though sonic virtual temperature is
     * not typically used for absolute temperature, and has its
     * own sources of uncertainty, mainly due to variability of the
     * transducer path length.
     *
     * It might be best to set the RH value or the Gamma_Slope to zero 
     * on the ATIK, so that it reports the virtual temp in dry air,
     * like the CSAT3. In this case the temperature at 340 m/s differs by
     * 0.4 degC using the two values of GAMMA_R.
     */
    static const float GAMMA_R = 403.242;

private:

    /**
     * Requested number of output wind variables.
     */
    int _numOut;

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
     * If the fraction of all missing counts is above this value,
     * then all output wind values are flagged.
     */
    float _diagThreshold;

    /**
     * Maximum angle of transducer shadow (aka flow distortion) corrections.
     * The angle of the raw, uncorrected wind vector with respect to each
     * transducer axis is computed. When the angle is less than
     * _maxShadowAngle, then the wind component along that axis is
     * increased by a factor of
     *
     * 1.0 / (1.0 - _shadowFactor + _shadowFactor * theta / _maxShadowAngle)
     *
     * The default value, and ATI's suggested value for _maxShadowAngle is
     * 70 degrees.
     *
     * This value can be set in the XML with a sensor parameter called
     * "maxShadowAngle".
     */
    float _maxShadowAngle;

    /**
     * Transducer shadow (aka flow distortion) correction factor.
     * The default value, and ATI's suggested value for _shadowFactor is 0.16.
     *
     * This value can be set in the XML with a sensor parameter called
     * "shadowFactor".
     */
    float _shadowFactor;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
