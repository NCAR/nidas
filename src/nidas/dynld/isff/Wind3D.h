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

    void setBias(int i, double val);

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

    void setMetek(int ismetek) {
        _metek = (ismetek == 1);
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
    
    bool _metek;

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

//namespace metek contains metek specific paramters
namespace metek {
  union uvwt { double array[4]; struct { double u; double v; double w; double t; }; };
  void CorrectTemperature(uvwt&);
  void Remove2DCorrections(uvwt&);
  void Apply3DCorrect(uvwt&);
  void Apply3DCorrect(float uvwtd[5]); 
  double CalcCorrection(double[6], double , const double[20][7]);

  //const double PI = 3.141592653589793; //nidas codebad calls this M_PI
  const double Degrees2Rad = 0.01745329251994329577; // === PI/180

  /* Orginal Metek correction for nc(alpha_raw, phi_raw). Note, the labels in the paper are incorrect*/
  const double mu_lut[20][7] = {
    //    C_0           C_3           S_3           C_6            S_6            C_9          S_9 phi (in degrees)
    {1.23095,   -0.0859199,   -0.0674271,    0.0160088,     0.0363397,     0.0141701,   -0.0271955}, //-50
    {1.19323,   -0.0430575,   0.00309311,    0.0430652,     0.0225135,   0.000740028,   -0.0114045}, //-45
    {1.17255,   -0.0206394,    0.0145473,    0.0399041,   -0.00592748,   -0.00650942,  -0.00762305}, //-40
    {1.15408,  -0.00768472,    0.0614486,    0.0382888,     0.0123096,    -0.0124673,  -0.00598534}, //-35
    {1.12616,  -5.36477e-6,    0.0636543,    0.0386879,     0.0153428,     -0.014148, -0.000210096}, //-30
    {1.09976,   0.00667086,    0.0705414,    0.0198549,     0.0165582,    -0.0114517,  -0.00115495}, //-25
    {1.07518,   0.00583915,    0.0591098,     0.011127,     0.0104259,   -0.00665653,   0.00119842}, //-20
    {1.05173,   0.00731099,    0.0527018,   0.00230123,    0.00587927,   -0.00229463,  -0.00297294}, //-15
    {1.02428,   0.00885121,    0.0330304, -0.000597029,    0.00340367,  -0.000745781, -0.000283634}, //-10
    {  1.011,   0.00930375,    0.0218448,   -0.0046575,    0.00203972,   -0.00112652,   0.00179908}, // -5
    {1.00672,    0.0105659,    0.0034918,  -0.00844128,    0.00228384,  -0.000824805,  0.000200667}, //  0
    {1.01053,   0.00885115,   -0.0182222,  -0.00894106,  -0.000719837,  -0.000420398,  -0.00049521}, //  5
    {1.02332,   0.00618183,    -0.035471,  -0.00455248,   -0.00215202,   -0.00229836, -0.000309162}, // 10
    {1.04358,   0.00648413,   -0.0494223,  0.000323015,   -0.00396036,   -0.00465476, -0.000117245}, // 15
    {1.06928,   0.00733521,   -0.0638425,    0.0101036,   -0.00829634,    -0.0073708,  -0.00051887}, // 20
    {1.09029,   0.00396333,   -0.0647836,    0.0187147,    -0.0126355,    -0.0115659,  0.000482614}, // 25
    {1.11877,   0.00299473,   -0.0661552,    0.0293485,   -0.00957493,   -0.00963845,    0.0029231}, // 30
    {1.13779,   0.00812517,   -0.0526322,    0.0341525,   -0.00971735,    -0.0114763,    0.0013481}, // 35
    {1.16659,  -0.00869651,   -0.0537855,    0.0290825,   -9.89207e-5,    -0.0133731,    0.0117738}, // 40
    {1.18695,   -0.0289647,   -0.0461693,     0.030231,    -0.0121524,   -0.00667729,   0.00565286}, // 45
  };

//alpha_lut is the alpha correction lookup table Values are in degrees. Note, the labels in the paper are incorrect
  const double alpha_lut[20][7] = {
    //    C_0            C_3            S_3            C_6          S_6          C_9           S_9 phi (in degrees)
    {-10.7681,        1.83694,      8.12521,       1.76476,   -0.120656,    -0.31818,      1.30896}, //-50
    {-7.57048,        2.25939,      4.22328,    -0.0394204,   -0.112215,   -0.289935,      1.99387}, //-45
    {-6.77725,       0.293479,      3.05333,      -1.16341,    0.433886,    0.207458,      1.05195}, //-40
    {-4.12528,        2.24741,     0.286582,     -0.936084,    0.205636,   -0.399336,      1.57736}, //-35
    {-2.00728,        3.63124,    -0.325198,     -0.821254,    0.236536,   -0.303478,     0.854497}, //-30
    {-3.1161,         3.91749,    -0.682098,     -0.274558,    0.401386,   -0.531782,     0.470723}, //-25
    {-1.73949,         3.5685,    -0.253107,     0.0306742,    0.236975,   -0.290767,    -0.224723}, //-20
    {-2.59966,         2.7604,    -0.425346,     0.0557135,   0.0392047,    0.222439,    -0.364683}, //-15
    {-1.80055,        2.02108,    -0.259729,      0.161799,    0.117651,    0.513197,   -0.0546757}, //-10
    {-1.02146,        1.22626,    -0.469781,     -0.177656,    0.402977,    0.408776,     0.513465}, // -5
    {0.152354,       0.208574,     0.051986,     -0.102825,    0.480597,  -0.0710578,     0.354821}, //  0
    {0.310938,      -0.703761,   -0.0131663,     0.0877815,    0.546872,   -0.342846,     0.176681}, //  5
    {0.530836,       -1.68132,   -0.0487515,     0.0553666,    0.524018,   -0.426562,   -0.0908979}, // 10
    {1.70881,        -2.46858,    -0.487399,      0.207364,    0.638065,   -0.458377,    -0.230826}, // 15
    {2.38137,        -3.37747,     0.026278,     0.0749961,    0.759096,    0.105791,    0.0287425}, // 20
    {3.81688,        -4.13918,    -0.690113,      0.170455,    0.474636,    0.424845,     0.232194}, // 25
    {3.49414,        -3.82687,    -0.229292,       0.54375,    0.322097,    0.387805,     0.823967}, // 30
    {4.1365,         -3.22485,     0.752425,      0.755442,    0.623119,    0.250988,      1.26713}, // 35
    {5.04661,        -2.53708,      1.23398,      0.623328,    0.653175,   -0.359131,      1.43131}, // 40
    {4.26165,        -3.12817,      2.61556,     0.0450348,   -0.330568,    -0.34354,      0.81789}, // 45
  };
  //phi_lut is the phi correction lookup table Values are in degrees. Note, the labels in the paper are incorrect
  const double phi_lut[20][7] = {
    //      C_0            C_3           S_3           C_6         S_6           C_9            S_9 phi (in degrees)
    {   5.77441,      -2.19044,     0.123475,    -0.229181,   0.226335,     0.271943,     0.0434668}, //-50
    {   3.82023,       -1.6847,     0.315654,     0.562738,   0.175507,   -0.0552129,     -0.110839}, //-45
    {   2.29783,      -1.04802,    0.0261005,     0.239236,   0.125053,    -0.310631,      0.388716}, //-40
    {   1.37922,       -1.0435,     0.302416,   -0.0112228,   0.333846,    -0.459678,      0.172019}, //-35
    {  0.837231,     -0.593247,    -0.199916,   -0.0591118,    0.19883,    -0.307377,      0.182622}, //-30
    {-0.0588021,    -0.0720115,      -0.6826,    -0.253726,   0.348259,    -0.322761,     0.0059973}, //-25
    {-0.0333721,      0.101664,     -1.41617,    -0.136743,   0.332169,    -0.244186,    -0.0612597}, //-20
    { 0.0423739,     0.0428399,     -1.90137,    -0.187419,   0.148025,      0.06782,    -0.0317571}, //-15
    {  0.318212,      0.126425,     -2.07763,   -0.0341571,   0.198621,     0.178598,      0.103543}, //-10
    {  0.721731,    -0.0274247,     -2.10221,    -0.081822,    0.36773,    0.0848013,      0.184226}, // -5
    {   1.65254,    -0.0582368,     -2.18993,   -0.0802346,   0.234886,   -0.0545883,    -0.0092531}, //  0
    {   2.49129,     -0.116475,     -2.11283,     0.112364,   0.247405,    -0.115218,    -0.0682998}, //  5
    {   2.99839,    -0.0867988,     -2.04382,     0.219581,   0.207231,   -0.0981521,    -0.0581594}, // 10
    {   3.55129,     -0.160112,      -1.8474,      0.22217,     0.2794,   -0.0323565,    -0.0951596}, // 15
    {   3.20977,     -0.137282,    -0.966014,     0.183032,   0.380154,     0.155093,    -0.0557369}, // 20
    {   3.38556,    -0.0596863,    -0.898053,      0.20526,    0.39357,     0.421141,   -0.00842409}, // 25
    {   3.18846,      0.266264,   -0.0951907,     0.166895,   0.373018,     0.338146,      0.187917}, // 30
    {   2.60134,      0.442007,     0.211612,    -0.114323,   0.359926,     0.224424,      0.209482}, // 35
    {   2.04655,       1.08915,     0.470385,    -0.333096,   0.268349,     0.263547,      0.264963}, // 40
    {  0.987659,       1.54127,     0.815214,    -0.504021, -0.0835985,     0.197387,     0.0819912}, // 45
  };
}; //namespace metek


}}}	// namespace nidas namespace dynld namespace isff

#endif
