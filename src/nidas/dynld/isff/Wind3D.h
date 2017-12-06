// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DNYLD_ISFF_WIND3D_H
#define NIDAS_DNYLD_ISFF_WIND3D_H

#include <nidas/core/SerialSensor.h>
#include <nidas/core/AdaptiveDespiker.h>
#include <nidas/Config.h>

#ifdef HAVE_LIBGSL
#include <gsl/gsl_linalg.h>
#endif

namespace nidas {

namespace dynld { namespace isff {

/**
 * Rotate a (U,V) 2D wind vector by an angle.
 * Typically used to correct winds for anemometer orientation,
 * rotating U,V from instrument coordinates to
 * geographic coordinates, where +U is wind to the east,
 * and +V is to the north.
 */
class WindRotator {
public:

    WindRotator();

    double getAngleDegrees() const;

    void setAngleDegrees(double val);

    void rotate(float* up, float* vp) const;

private:

    double _angle;

    double _sinAngle;

    double _cosAngle;
};

/**
 * WindTilter is used to apply a 3d rotation matrix to a wind vector.
 *
 * WindTilter generates a matrix for rotating 3d wind
 * vectors from an orthogonal sonic coordinate system to a new coordinate
 * system whose w axis is defined by two angles, lean and leanaz.  It is
 * commonly used to rotate sonic wind data to mean flow
 * coordinates, whose w axis is normal to the plane of the observed flow.
 *
 * lean is the angle in radians between the flow normal and the
 * sonic w axis (0 <= lean <= 180)
 *
 * leanaz is the clock angle in radians from the sonic u axis to the
 * projection of the flow normal in the sonic uv plane, positive
 * toward the sonic v axis. (0 <= leanaz < 360)
 *
 * The unit axes of sonic coordinates:		Us, Vs, Ws
 * The unix axes of the flow coordiate system:	Uf, Vf, Wf
 *
 * This is primarily a rotation of the W axis. The Uf axis
 * remains in the plane of Us and an "up" direction.
 *
 * In sonic (aka instrument coordinates) the Wf axis is
 *      sin(lean) * cos(leanaz)
 *      sin(lean) * sin(leanaz)
 *      cos(lean)
 *
 * Once you have the Wf axis defining the Uf,Vf plane, there is some
 * uncertainty about where to put the Uf axis on that plane.
 *
 * If Uf is the intersection of the normal plane with the old Us,Ws 
 * plane, then
 * Uf = Vs X Wf		cross product of Vs and Wf
 *    = (0 1 0) X Wf	In sonic coords Vs is just (0 1 0)
 *			Then normalize Uf.
 * The above method is used when UP_IS_SONIC_W == true
 *
 * If Uf is the intersection of the flow normal plane with the
 * Wf,Us plane, then
 *   
 *  Uf = (Wf X Us) X Wf 	(normalized)
 *  
 * The above method is used when UP_IS_SONIC_W == false
 *
 * Right now the default value of UP_IS_SONIC_W is false.
 *
 * I suppose if you had your sonic on the side
 * of a steep hill you may want "up" to be the sonic W axis.
 *
 * **********************************************************************
 * How to determine the flow normal from sonic wind data.
 *
 * The value of W in flow coordinates (Wf) is the dot product of
 * the corrected sonic wind vector (us-uoff,vs-voff,ws-woff) with the
 * Wf unit vector, in sonic coordinates.
 * uoff, voff and woff are the sonic biases.
 *
 * Wf = (us-uoff) * sin(lean)cos(leanaz) + (vs-voff)*sin(lean)sin(leanaz)
 *  + (ws-woff) * cos(lean)
 *
 * An averaged Wf is by definition zero, which gives an equation
 * for wsbar in terms of usbar and vsbar.
 *
 * wsbar = woff - (usbar-uoff)*tan(lean)cos(leanaz)
 *	 - (vsbar-voff)*tan(lean)sin(leanaz)
 *
 * Therefore wsbar should look like a linear function of the corrected
 * and averaged U and V components from the sonic, (uscor=usbar-uoff):
 *
 *  wsbar = b1 + b2 * uscor + b3 * vscor
 *
 * One can determine these b coefficients with a minimum variance
 * fit, and
 *
 *  b1 = woff
 *  b2 = -tan(lean)cos(leanaz)
 *  b3 = -tan(lean)sin(leanaz)
 *
 * lean and leanaz are therefore:
 * lean = atan(sqrt(b2*b2 + b3*b3))
 * leanaz = atan2(-b3,-b2)
 *
 * @version $Revision$
 * @author  $Author$
 */
class WindTilter {
public:

    WindTilter();

    double getLeanDegrees() const
    {
	return _lean * 180.0 / M_PI;
    }
    void setLeanDegrees(double val)
    {
	_lean = val * M_PI / 180.0;
	computeMatrix();
    }

    double getLeanAzimuthDegrees() const
    {
	return _leanaz * 180.0 / M_PI;
    }
    void setLeanAzimuthDegrees(double val)
    {
	_leanaz = val * M_PI / 180.;
	if (!_identity) computeMatrix();
    }

    bool isIdentity() const
    {
        return _identity;
    }

    void rotate(float*u, float*v, float*w) const;

private:

    void computeMatrix();

    double _lean;

    double _leanaz;

    bool _identity;

    double _mat[3][3];

    bool UP_IS_SONIC_W;

};

/**
 * A class for performing the common processes on
 * wind data from a 3D sonic anemometer.
 */
class Wind3D: public nidas::core::SerialSensor
{
public:

    Wind3D();

    ~Wind3D();

    /**
     * Basic process method for sonic anemometer wind plus temperature:
     * u,v,w,tc parsed from an ASCII sample. Applies despiking,
     * orientation corrections, bias, tilts and horizontal rotations, as configured.
     */
    bool process(const nidas::core::Sample* samp, 
        std::list<const nidas::core::Sample*>& results) throw();

    void setBias(int i,double val)
    {
        if (i >= 0 && i < 3) _bias[i] = val;
    }

    double getBias(int i) const
    {
        return _bias[i];
    }

    double getVazimuth() const
    {
	return _rotator.getAngleDegrees();
    }

    /**
     * Wind vectors in geographic coordinates are expressed
     * by U, the component of the wind blowing toward the east,
     * and V, the component of the wind blowing toward the north.
     * If the V axis of a sonic anemometer is pointing
     * north then no rotation is necessary to convert from
     * sonic coordinates to geographic.  So the compass azimuth
     * (0=north,90=east, etc) of the sonic +V axis is the
     * angle between geographic and sonic coordinates.
     */
    void setVazimuth(double val)
    {
	_rotator.setAngleDegrees(val);
    }

    double getLeanDegrees() const
    {
	return _tilter.getLeanDegrees();
    }
    void setLeanDegrees(double val)
    {
	_tilter.setLeanDegrees(val);
    }

    double getLeanAzimuthDegrees() const
    {
	return _tilter.getLeanAzimuthDegrees();
    }
    void setLeanAzimuthDegrees(double val)
    {
	_tilter.setLeanAzimuthDegrees(val);
    }

    void setDespike(bool val)
    {
        _despike = val;
    }

    bool getDespike() const
    {
        return _despike;
    }

    void setOutlierProbability(double val)
    {
	for (int i = 0; i < 4; i++)
	    _despiker[i].setOutlierProbability(val);
    }

    double getOutlierProbability() const
    {
        return _despiker[0].getOutlierProbability();
    }

    void setDiscLevelMultiplier(double val)
    {
	for (int i = 0; i < 4; i++)
	    _despiker[i].setDiscLevelMultiplier(val);
    }

    double getDiscLevelMultiplier() const
    {
        return _despiker[0].getDiscLevelMultiplier();
    }

    double getDiscLevel() const
    {
        return _despiker[0].getDiscLevel();
    }

    void setTcOffset(double val) 
    {
        _tcOffset = val;
    }

    double getTcOffset() const
    {
        return _tcOffset;
    }

    void setTcSlope(double val) 
    {
        _tcSlope = val;
    }

    double getTcSlope() const
    {
        return _tcSlope;
    }

    /**
     * Should 2D horizontal rotations of U,V be applied?
     */
    void setDoHorizontalRotation(bool val)
    {
        _horizontalRotation = val;
    }
        
    /**
     * Should 3D tilt corrections be applied?
     */
    void setDoTiltCorrection(bool val)
    {
        _tiltCorrection = val;
    }

    void despike(nidas::core::dsm_time_t tt,float* uvwt,int n, bool* spikeOrMissing)
    	throw();

    /**
     * Do standard bias removal, tilt correction and horizontal rotation of
     * 3d sonic anemometer data.
     *
     * @tt time tag of the data, used to search for a parameter
     *    a file containing a calibration time series.
     * @param uvwt Pointer to an array of 4 floats, containing
     *    u,v,w and tc(virtual temperature). New values
     *    are written back via the pointers.
     */
    void offsetsTiltAndRotate(nidas::core::dsm_time_t tt, float* uvwt) throw();

    /**
     * Apply orientation changes to the wind components.
     **/
    void applyOrientation(nidas::core::dsm_time_t tt, float* uvwt) throw();

    /**
     * Update the settings from the offsets and angles calibration file, if
     * any.
     **/
    void readOffsetsAnglesCalFile(nidas::core::dsm_time_t tt) throw();

    /**
     * Validate the configuration of this sensor. Calls the base class
     * validate(), parseParameters(), and checkSampleTags().
     */
    void validate()
	throw(nidas::util::InvalidParameterException);

    /**
     * Warn user if number of scanf fields does not match
     * number expected from variables in sample.
     */
    void validateSscanfs() throw(nidas::util::InvalidParameterException);

    /**
     * Parse the orientation parameter and set the vectors which translate
     * the axes and signs of the wind sensor components.  The parameter
     * must be one string: 'normal' (default), 'down', 'lefthanded',
     * 'flipped' or 'horizontal'.  Throws InvalidParameterException if the
     * string cannot be parsed.
     **/
    void
    setOrientation(const std::string& orientation);

    /**
     * Parse the list of nidas::core::Parameter that are associated with this sensor.
     * This also checks the parameters "wind3d_tilt_correction",
     * and "wind3d_horiz_rotation" that may have been set on the Project
     * singleton, calls setDoTiltCorrection() and setDoHorizontalRotation()
     * accordingly.
     */
    virtual void parseParameters() throw(nidas::util::InvalidParameterException);

    /**
     * Check the SampleTags that are defined for this sensor.
     */
    virtual void checkSampleTags() throw(nidas::util::InvalidParameterException);

#ifdef HAVE_LIBGSL
    /**
     * Read 3x3 matrix to be used for the transformation of wind vectors in ABC
     * transducer coordinates to orthoganal UVW coordinates. These values are typically
     * in a CalFile.
     */
    virtual void getTransducerRotation(nidas::core::dsm_time_t tt) throw();

    virtual void transducerShadowCorrection(nidas::core::dsm_time_t, float *) throw();
#endif

protected:

    static const int DATA_GAP_USEC = 60000000;

    nidas::core::dsm_time_t _ttlast[4];

    double _bias[3];

    bool _allBiasesNaN;

    bool _despike;

    nidas::core::AdaptiveDespiker _despiker[4];
 
    WindRotator _rotator;

    WindTilter _tilter;

    double _tcOffset;

    double _tcSlope;

    /**
     * Should horizontal rotation of U,V be performed?
     */
    bool _horizontalRotation;

    /**
     * Should 3D tilt correction be applied?
     */
    bool _tiltCorrection;

    /**
     * Id of output sample.
     */
    nidas::core::dsm_sample_id_t _sampleId;

    /**
     * If user requests "diag" or "status", its index
     * in the output sample.
     */
    int _diagIndex;

    /**
     * If user requests "ldiag", its index
     * in the output sample.
     */
    int _ldiagIndex;

    /**
     * If user requests wind speed, variable name "spd", its index
     * in the output sample.
     */
    int _spdIndex;

    /**
     * If user requests wind direction, variable name "dir", its index
     * in the output sample.
     */
    int _dirIndex;

    unsigned int _noutVals;

    /**
     * Number of variables that are parsed from input, i.e.
     * not derived.
     */
    unsigned int _numParsed;

    /**
     * CalFile containing wind offsets and rotation angles.
     */
    nidas::core::CalFile* _oaCalFile;

    /**
     * Is the sonic oriented in a unusual way, e.g. upside-down, etc?
     */
    bool _unusualOrientation;

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

#ifdef HAVE_LIBGSL
    /**
     * CalFile containing the transducer geometry matrix for rotation
     * to transducer coordinates, which is necessary for transducer
     * shadowing correction.
     */
    nidas::core::CalFile* _atCalFile;

    /**
     * Axes transformation matrix, from non-orthogonal ABC to orthogonal UVW coordinates.
     */
    double _atMatrix[3][3];

#define COMPUTE_ABC2UVW_INVERSE
#ifdef COMPUTE_ABC2UVW_INVERSE
    double _atInverse[3][3];
#else
    gsl_vector* _atVectorGSL1;
    gsl_vector* _atVectorGSL2;
#endif

    gsl_matrix* _atMatrixGSL;

    gsl_permutation* _atPermutationGSL;
#endif

    /**
     * Transducer shadow (aka flow distortion) correction factor.
     * This value can be set in the XML with a sensor parameter called
     * "shadowFactor".
     */
    double _shadowFactor;

private:

    // no copying
    Wind3D(const Wind3D& x);

    // no assignment
    Wind3D& operator=(const Wind3D& x);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
