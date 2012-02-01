// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DNYLD_ISFF_SONICANEMOMETER_H
#define NIDAS_DNYLD_ISFF_SONICANEMOMETER_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/AdaptiveDespiker.h>

namespace nidas { namespace dynld { namespace isff {

class WindRotator {
public:

    WindRotator();

    float getAngleDegrees() const;

    void setAngleDegrees(float val);

    void rotate(float* up, float* vp) const;

private:

    float _angle;

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
 * There is some uncertainty regarding what's "up", though
 * it usually makes so little difference that it's probably not worth
 * worrying about.  This code assumes "up" is Wf.
 * I suppose if you had your sonic on the side
 * of a steep hill you may want "up" to be the sonic W axis.
 *
 * If "up" is the flow normal, then
 *   
 *  Uf = (Wf X Us) X Wf 	(normalized)
 *  
 * If "up" is the sonic W axis, then 
 *
 * Uf = Vs X Wf		cross product of Vs and Wf
 *    = (0 1 0) X Wf	In sonic coords Vs is just (0 1 0)
 *			Then normalize Uf.
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

    float getLeanDegrees() const
    {
	return (float)(_lean * 180.0 / M_PI);
    }
    void setLeanDegrees(float val)
    {
	_lean = (float)(val * M_PI / 180.0);
	computeMatrix();
    }

    float getLeanAzimuthDegrees() const
    {
	return (float)(_leanaz * 180.0 / M_PI);
    }
    void setLeanAzimuthDegrees(float val)
    {
	_leanaz = (float)(val * M_PI / 180.);
	if (!_identity) computeMatrix();
    }

    bool isIdentity() const
    {
        return _identity;
    }

    void rotate(float*u, float*v, float*w) const;

private:

    void computeMatrix();

    float _lean;

    float _leanaz;

    bool _identity;

    double _mat[3][3];

    bool UP_IS_SONIC_W;

};

/**
 * A class for performing the common processes on
 * wind data from a 3D sonic anemometer.
 */
class SonicAnemometer: public DSMSerialSensor
{
public:

    SonicAnemometer();

    ~SonicAnemometer() {}

    void setBias(int i,float val)
    {
        if (i >= 0 && i < 3) _bias[i] = val;
    }

    float getBias(int i) const
    {
        return _bias[i];
    }

    float getVazimuth() const
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
    void setVazimuth(float val)
    {
	_rotator.setAngleDegrees(val);
    }

    float getLeanDegrees() const
    {
	return _tilter.getLeanDegrees();
    }
    void setLeanDegrees(float val)
    {
	_tilter.setLeanDegrees(val);
    }

    float getLeanAzimuthDegrees() const
    {
	return _tilter.getLeanAzimuthDegrees();
    }
    void setLeanAzimuthDegrees(float val)
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

    void setOutlierProbability(float val)
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

    void setTcOffset(float val) 
    {
        _tcOffset = val;
    }

    float getTcOffset() const
    {
        return _tcOffset;
    }

    void setTcSlope(float val) 
    {
        _tcSlope = val;
    }

    float getTcSlope() const
    {
        return _tcSlope;
    }

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    void despike(dsm_time_t tt,float* uvwt,int n, bool* spikeOrMissing)
    	throw();
    /**
     * Do standard processing of 3d sonic anemometer data.
     * @tt time tag of the data, used to search for a parameter
     *    a file containing a calibration time series.
     * @param uvwt Pointer to an array of 4 floats, containing
     *    u,v,w and tc(virtual temperature).  u,v,w are
     *    and rotated, based on attributes of SonicAnemometer.
     */
    void offsetsAndRotate(dsm_time_t tt,float* uvwt) throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    static const int DATA_GAP_USEC = 60000000;

    dsm_time_t _ttlast[4];

    float _bias[3];

    bool _allBiasesNaN;

    bool _despike;

    AdaptiveDespiker _despiker[4];
 
    WindRotator _rotator;

    WindTilter _tilter;

    dsm_time_t _calTime;

    float _tcOffset;

    float _tcSlope;
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
