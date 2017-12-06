/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include "Wind3D.h"
#include <nidas/core/PhysConstants.h>
#include <nidas/core/AsciiSscanf.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Project.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,Wind3D)

Wind3D::Wind3D():
    _allBiasesNaN(false),
    _despike(false),
    _rotator(),_tilter(),
    _tcOffset(0.0),_tcSlope(1.0),
    _horizontalRotation(true),_tiltCorrection(true),
    _sampleId(0),
    _diagIndex(-1), _ldiagIndex(-1),
    _spdIndex(-1), _dirIndex(-1),
    _noutVals(0),
    _numParsed(0),
    _oaCalFile(0), _unusualOrientation(false),
#ifdef HAVE_LIBGSL
    _atCalFile(0),
    _atMatrix(),
#ifdef COMPUTE_ABC2UVW_INVERSE
    _atInverse(),
#else
    _atVectorGSL1(gsl_vector_alloc(3)),
    _atVectorGSL2(gsl_vector_alloc(3)),
#endif
    _atMatrixGSL(gsl_matrix_alloc(3,3)),
    _atPermutationGSL(gsl_permutation_alloc(3)),
#endif
    _shadowFactor(0.0)
{
    for (int i = 0; i < 3; i++) {
	_bias[i] = 0.0;
    }
    for (int i = 0; i < 4; i++) {
	_ttlast[i] = 0;
    }

    /* index and sign transform for usual sonic orientation.
     * Normal orientation, no component change: 0 to 0, 1 to 1 and 2 to 2,
     * with no sign change. */
    for (int i = 0; i < 3; i++) {
        _tx[i] = i;
        _sx[i] = 1;
    }
}

Wind3D::~Wind3D()
{
#ifdef HAVE_LIBGSL
#ifndef COMPUTE_ABC2UVW_INVERSE
    gsl_vector_free(_atVectorGSL1);
    gsl_vector_free(_atVectorGSL2);
#endif
    gsl_matrix_free(_atMatrixGSL);
    gsl_permutation_free(_atPermutationGSL);
#endif
}
void Wind3D::despike(dsm_time_t tt,
	float* uvwt,int n,bool* spikeOrMissing) throw()
{
    vector<double> dvec(n);	// despiked data
    double* duvwt = &dvec.front();
    /*
     * Despike data
     */
    for (int i=0; i < n; i++) {
        /* Restart statistics after data gap. */
        if (tt - _ttlast[i] > DATA_GAP_USEC) _despiker[i].reset();

        /* Despike status, 1=despiked, 0=not despiked */
        spikeOrMissing[i] = isnan(uvwt[i]);
        duvwt[i] = _despiker[i].despike(uvwt[i],spikeOrMissing+i);
        if (!spikeOrMissing[i]) _ttlast[i] = tt;
    }

    // If user wants despiked data, copy results back to passed array.
    if (getDespike()) {
        for (int i=0; i < n; i++) {
            uvwt[i] = duvwt[i];
        }
    }
}


void Wind3D::readOffsetsAnglesCalFile(dsm_time_t tt) throw()
{
    // Read CalFile of bias and rotation angles.
    // u.off   v.off   w.off    theta  phi    Vazimuth  t.off t.slope
    if (_oaCalFile) {
        while(tt >= _oaCalFile->nextTime().toUsecs()) {
            float d[8];
            try {
                n_u::UTime calTime;
                int n = _oaCalFile->readCF(calTime, d,sizeof d/sizeof(d[0]));
                for (int i = 0; i < 3 && i < n; i++) setBias(i,d[i]);
                int nnan = 0;
                for (int i = 0; i < 3; i++) if (isnan(getBias(i))) nnan++;
                _allBiasesNaN = (nnan == 3);

                if (n > 3) setLeanDegrees(d[3]);
                if (n > 4) setLeanAzimuthDegrees(d[4]);
                if (n > 5) setVazimuth(d[5]);
                /* Old versions of CSAT calibration files contained
                 * a byteShift value in column 7 - which this version
                 * of the software does not support.  This software
                 * instead will expect column 7 and 8 to contain
                 * the offset and slope for the correction to virtual
                 * temperature.
                 */
                if (n > 7) {
                    setTcOffset(d[6]);
                    setTcSlope(d[7]);
                }
            }
            catch(const n_u::EOFException& e)
            {
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    _oaCalFile->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < 3; i++) setBias(i,floatNAN);
                setLeanDegrees(floatNAN);
                setLeanAzimuthDegrees(floatNAN);
                setVazimuth(floatNAN);
                _oaCalFile = 0;
                break;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    _oaCalFile->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < 3; i++) setBias(i,floatNAN);
                setLeanDegrees(floatNAN);
                setLeanAzimuthDegrees(floatNAN);
                setVazimuth(floatNAN);
                _oaCalFile = 0;
                break;
            }
        }
    }
}

void Wind3D::applyOrientation(dsm_time_t tt, float* uvwt) throw()
{
    readOffsetsAnglesCalFile(tt);

    if (_unusualOrientation)
    {
        float dn[3];
        for (int i = 0; i < 3; i++)
        {
            dn[i] = _sx[i] * uvwt[_tx[i]];
        }
        memcpy(uvwt, dn, sizeof(dn));
    }
}

void Wind3D::offsetsTiltAndRotate(dsm_time_t tt, float* uvwt) throw()
{
    readOffsetsAnglesCalFile(tt);

    // bias removal is part of the tilt correction.
    if (_tiltCorrection) {
        for (int i=0; i<3; i++) uvwt[i] -= _bias[i];
        if (!_tilter.isIdentity()) _tilter.rotate(uvwt,uvwt+1,uvwt+2);
    }
    // Apply correction to Tc
    uvwt[3] = uvwt[3] * _tcSlope + _tcOffset;
    if (_horizontalRotation) _rotator.rotate(uvwt,uvwt+1);
}

void Wind3D::validate() throw(n_u::InvalidParameterException)
{
    SerialSensor::validate();

    parseParameters();

    checkSampleTags();
}

void
Wind3D::
setOrientation(const std::string& orientation)
{
    /* _tx and _sx are used in the calculation of a transformed wind
     * vector as follows:
     *
     * for i = 0,1,2
     *     dout[i] = _sx[i] * wind_in[_tx[i]]
     * where:
     *  dout[0,1,2] are the new, transformed U,V,W
     *  wind_in[0,1,2] are the original U,V,W in raw sonic coordinates
     *
     *  When the sonic is in the normal orientation, +w is upwards
     *  approximately w.r.t gravity, and +u is wind into the sonic array.
     */
    DLOG(("") << getName() << " setting orientation to " << orientation);
    if (orientation == "normal")
    {
        _tx[0] = 0;
        _tx[1] = 1;
        _tx[2] = 2;
        _sx[0] = 1;
        _sx[1] = 1;
        _sx[2] = 1;
        _unusualOrientation = false;
    }
    else if (orientation == "down")
    {
        /* For flow-distortion experiments, the sonic may be mounted 
         * pointing down. This is a 90 degree "down" rotation about the
         * sonic v axis, followed by a 180 deg rotation about the sonic u axis,
         * flipping the sign of v.  Transform the components so that the
         * new +w is upwards wrt gravity.
         * new    raw sonic
         * u      w
         * v      -v
         * w      u
         */
        _tx[0] = 2;     // new u is raw sonic w
        _tx[1] = 1;     // v is raw sonic -v
        _tx[2] = 0;     // new w is raw sonic u
        _sx[0] = 1;
        _sx[1] = -1;    // v is -v
        _sx[2] = 1;
        _unusualOrientation = true;
    }
    else if (orientation == "lefthanded")
    {
        /* If wind direction is measured counterclockwise, convert to 
         * clockwise (dir = 360 - dir). This is done by negating the v 
         * component.
         * new    raw sonic
         * u      u
         * v      -v
         * w      w
         */
        _tx[0] = 0;
        _tx[1] = 1;
        _tx[2] = 2;
        _sx[0] = 1;
        _sx[1] = -1; //v is -v
        _sx[2] = 1;
        _unusualOrientation = true;
    }
    else if (orientation == "flipped")
    {
        /* Sonic flipped over, a 180 deg rotation about sonic u axis.
         * Change sign on v,w:
         * new    raw sonic
         * u      u
         * v      -v
         * w      -w
         */
        _tx[0] = 0;
        _tx[1] = 1;
        _tx[2] = 2;
        _sx[0] = 1;
        _sx[1] = -1;
        _sx[2] = -1;
        _unusualOrientation = true;
    }
    else if (orientation == "horizontal")
    {
        /* Sonic flipped on its side. For CSAT3, the labelled face of  the
         * "junction box" faces up.
         * Looking "out" from the tower in the -u direction, this is a 90 deg CC
         * rotation about the u axis, so no change to u,
         * new w is sonic v (sonic v points up), new v is sonic -w.
         * new    raw sonic
         * u      u
         * v      -w
         * w      v
         */
        _tx[0] = 0;
        _tx[1] = 2;
        _tx[2] = 1;
        _sx[0] = 1;
        _sx[1] = -1;
        _sx[2] = 1;
        _unusualOrientation = true;
    }
    else
    {
        throw n_u::InvalidParameterException
            (getName(), "orientation parameter",
             "must be one string: 'normal' (default), 'down', 'lefthanded', "
             "'flipped' or 'horizontal'");
    }
    float before[3] = { 1.0, 2.0, 3.0 };
    float after[3] = { 1.0, 2.0, 3.0 };
    
    applyOrientation(0, after);
    DLOG(("sonic wind orientation will convert (%g,%g,%g) to (%g,%g,%g)",
          before[0], before[1], before[2], after[0], after[1], after[2]));
}



void Wind3D::parseParameters()
    throw(n_u::InvalidParameterException)
{
    // Set default values of these parameters from the Project if they exist.
    // The value can be overridden with sensor parameters, below.
    const Project* project = Project::getInstance();

    const Parameter* parameter =
        Project::getInstance()->getParameter("wind3d_horiz_rotation");
    if (parameter) {
        if ((parameter->getType() != Parameter::BOOL_PARAM &&
            parameter->getType() != Parameter::INT_PARAM &&
            parameter->getType() != Parameter::FLOAT_PARAM) ||
            parameter->getLength() != 1)
            throw n_u::InvalidParameterException(getName(),parameter->getName(),
                "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
        bool val = (bool) parameter->getNumericValue(0);
        setDoHorizontalRotation(val);
    }

    parameter = Project::getInstance()->getParameter("wind3d_tilt_correction");
    if (parameter) {
        if ((parameter->getType() != Parameter::BOOL_PARAM &&
            parameter->getType() != Parameter::INT_PARAM &&
            parameter->getType() != Parameter::FLOAT_PARAM) ||
            parameter->getLength() != 1)
            throw n_u::InvalidParameterException(getName(),parameter->getName(),
                "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
        bool val = (bool) parameter->getNumericValue(0);
        setDoTiltCorrection(val);
    }

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    _allBiasesNaN = false;

    for ( ; pi != params.end(); ++pi) {
        parameter = *pi;

        if (parameter->getName() == "biases") {
            int nnan = 0;
            for (int i = 0; i < 3 && i < parameter->getLength(); i++) {
                setBias(i,parameter->getNumericValue(i));
                if (isnan(getBias(i))) nnan++;
            }
            if (nnan == 3) _allBiasesNaN = true;
        }
        else if (parameter->getName() == "Vazimuth") {
            setVazimuth(parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "despike") {
            setDespike(parameter->getNumericValue(0) != 0.0);
        }
        else if (parameter->getName() == "outlierProbability") {
            setOutlierProbability(parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "discLevelMultiplier") {
            setDiscLevelMultiplier(parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "lean") {
            setLeanDegrees(parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "leanAzimuth") {
            setLeanAzimuthDegrees(parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "wind3d_horiz_rotation") {
            if ((parameter->getType() != Parameter::BOOL_PARAM &&
                parameter->getType() != Parameter::INT_PARAM &&
                parameter->getType() != Parameter::FLOAT_PARAM) ||
                parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),parameter->getName(),
                    "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
            setDoHorizontalRotation((bool)parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "wind3d_tilt_correction") {
            if ((parameter->getType() != Parameter::BOOL_PARAM &&
                parameter->getType() != Parameter::INT_PARAM &&
                parameter->getType() != Parameter::FLOAT_PARAM) ||
                parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),parameter->getName(),
                    "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
            setDoTiltCorrection((bool)parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "orientation") {
            if (parameter->getType() == Parameter::STRING_PARAM &&
                parameter->getLength() == 1)
            {
                setOrientation(project->expandString(parameter->getStringValue(0)));
            }
            else
            {
                throw n_u::InvalidParameterException
                    (getName(), parameter->getName(),
                    "must be a string parameter of length 1");
            }
        }
        else if (parameter->getName() == "shadowFactor") {
            if (parameter->getType() != Parameter::FLOAT_PARAM ||
                    parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                            "shadowFactor","must be one float");
#ifdef HAVE_LIBGSL
            _shadowFactor = parameter->getNumericValue(0);
#else
            if (parameter->getNumericValue(0) != 0.0)
                    throw n_u::InvalidParameterException(getName(),
                        "shadowFactor","must be zero since there is no GSL support");
#endif
        }
        else if (parameter->getName() == "oversample");
        else if (parameter->getName() == "soniclog");
        else if (parameter->getName() == "maxShadowAngle");
        else if (parameter->getName() == "expectedCounts");
        else if (parameter->getName() == "maxMissingFraction");
        else if (parameter->getName() == "bandwidth");
        else if (parameter->getName() == "configure");
        else if (parameter->getName() == "checkCounter");
        else throw n_u::InvalidParameterException(
             getName(),"parameter",parameter->getName());
    }

    _oaCalFile =  getCalFile("offsets_angles");
    if (!_oaCalFile) _oaCalFile =  getCalFile("");

#ifdef HAVE_LIBGSL
    // transformation matrix from non-orthogonal axes to UVW
    _atCalFile = getCalFile("abc2uvw");
#endif
}

void Wind3D::checkSampleTags()
    throw(n_u::InvalidParameterException)
{

    list<SampleTag*>& tags= getSampleTags();
    list<SampleTag*>::const_iterator si = tags.begin();

    _noutVals = 4;  // u,v,w,tc
    _numParsed = 4;  // u,v,w,tc

    for ( ; si != tags.end(); ++si) {
        const SampleTag* stag = *si;
        _sampleId = stag->getId();

        VariableIterator vi = stag->getVariableIterator();
        for (unsigned int i = 0; vi.hasNext(); i++) {
            const Variable* var = vi.next();
            const string& vname = var->getName();
            if (vname.length() > 2 && vname.substr(0,3) == "spd") {
                _spdIndex = i;
                if (i >= _noutVals) _noutVals = i + 1;
            }
            else if (vname.length() > 2 && vname.substr(0,3) == "dir") {
                _dirIndex = i;
                if (i >= _noutVals) _noutVals = i + 1;
            }
            else if (vname == "diag" || (vname.length() > 4 && vname.substr(0,5) == "diag.")) {
                _diagIndex = i;
                if (i >= _noutVals) _noutVals = i + 1;
                if (i >= _numParsed) _numParsed = i + 1;
            }
            else if (vname == "status" || (vname.length() > 5 && vname.substr(0,6) == "status.")) {
                _diagIndex = i;
                if (i >= _noutVals) _noutVals = i + 1;
            }
            else if (vname == "ldiag" || (vname.length() > 5 && vname.substr(0,6) == "ldiag.")) {
                _ldiagIndex = i;
                if (i >= _noutVals) _noutVals = i + 1;
            }
        }
    }
}

void Wind3D::validateSscanfs() throw(n_u::InvalidParameterException)
{
    const std::list<AsciiSscanf*>& sscanfers = getScanfers();

    // binary sensor
    if (sscanfers.empty()) return;

    std::list<AsciiSscanf*>::const_iterator si = sscanfers.begin();

    for ( ; si != sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
        unsigned int nf = sscanf->getNumberOfFields();

        if (nf < _numParsed) {
            ostringstream ost;
            ost << "number of scanf fields (" << nf <<
                ") is less than the number expected (" << _numParsed << ")";
            throw n_u::InvalidParameterException(getName(),"scanfFormat",ost.str());
        }
    }
}

#ifdef HAVE_LIBGSL
void Wind3D::transducerShadowCorrection(dsm_time_t tt,float* uvw) throw()
{
    if (!_atCalFile || _shadowFactor == 0.0 || isnan(_atMatrix[0][0])) return;

    double spd2 = uvw[0] * uvw[0] + uvw[1] * uvw[1] + uvw[2] * uvw[2];

    /* If one component is missing, do we mark all as missing?
     * This should not be a common occurance, but since this data
     * is commonly averaged, it wouldn't be obvious in the averages
     * whether some values were not being shadow corrected. So we'll
     * let one NAN "spoil the barrel".
     */
    if (isnan(spd2)) {
        for (int i = 0; i < 3; i++) uvw[i] = floatNAN;
        return;
    }

    getTransducerRotation(tt);

    double abc[3];

#ifdef COMPUTE_ABC2UVW_INVERSE
    // rotate from UVW to non-orthogonal transducer coordinates, ABC
    for (int i = 0; i < 3; i++) {
        abc[i] = 0.0;
        for (int j = 0; j < 3; j++)
            abc[i] += uvw[j] * _atInverse[i][j];
    }
#else
    // solve the equation for abc:
    // matrix * abc = uvw

    for (int i = 0; i < 3; i++)
        gsl_vector_set(_atVectorGSL1,i,uvw[i]);

    gsl_linalg_LU_solve(_atMatrixGSL, _atPermutationGSL, _atVectorGSL1, _atVectorGSL2);

    for (int i = 0; i < 3; i++)
        abc[i] = gsl_vector_get(_atVectorGSL2,i);
#endif

    // apply shadow correction to winds in transducer coordinates
    for (int i = 0; i < 3; i++) {
        double x = abc[i];
        double sintheta = ::sqrt(1.0 - x * x / spd2);
        abc[i] = x / (1.0 - _shadowFactor + _shadowFactor * sintheta);
    }

    // cerr << "uvw=" << uvw[0] << ' ' << uvw[1] << ' ' << uvw[2] << endl;

    // rotate back to uvw coordinates
    for (int i = 0; i < 3; i++) {
        uvw[i] = 0.0;
        for (int j = 0; j < 3; j++)
            uvw[i] += abc[j] * _atMatrix[i][j];
    }

    // cerr << "uvw=" << uvw[0] << ' ' << uvw[1] << ' ' << uvw[2] << endl;
}

void Wind3D::getTransducerRotation(dsm_time_t tt) throw()
{
    if (_atCalFile) {
        while(tt >= _atCalFile->nextTime().toUsecs()) {

            try {
                n_u::UTime calTime;
                float data[3*3];
                int n = _atCalFile->readCF(calTime, data,sizeof(data)/sizeof(data[0]));
                if (n != 9) {
                    if (n != 0)
                        WLOG(("%s: short record of less than 9 values at line %d",
                            _atCalFile->getCurrentFileName().c_str(),
                            _atCalFile->getLineNumber()));
                    continue;
                }
                const float* dp = data;
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        gsl_matrix_set(_atMatrixGSL,i,j,*dp);
                        _atMatrix[i][j] = *dp++;
                    }
                    // cerr << _atMatrix[i][0] << ' ' << _atMatrix[i][1] << ' ' << _atMatrix[i][2] << endl;
                }
                int sign;
                gsl_linalg_LU_decomp(_atMatrixGSL,_atPermutationGSL, &sign);
#ifdef COMPUTE_ABC2UVW_INVERSE
                gsl_matrix* inverseGSL = gsl_matrix_alloc(3,3);
                gsl_linalg_LU_invert(_atMatrixGSL,_atPermutationGSL, inverseGSL);
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        _atInverse[i][j] = gsl_matrix_get(inverseGSL,i,j);
                    }
                }
                gsl_matrix_free(inverseGSL);
#endif
            }
            catch(const n_u::EOFException& e)
            {
            }
            catch(const n_u::IOException& e)
            {
                WLOG(("%s: %s", _atCalFile->getCurrentFileName().c_str(),e.what()));
                _atMatrix[0][0] = floatNAN;
                _atCalFile = 0;
                break;
            }
            catch(const n_u::ParseException& e)
            {
                WLOG(("%s: %s", _atCalFile->getCurrentFileName().c_str(),e.what()));
                _atMatrix[0][0] = floatNAN;
                _atCalFile = 0;
                break;
            }
        }
    }
}
#endif

bool Wind3D::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    std::list<const Sample*> parseResults;

    SerialSensor::process(samp,parseResults);

    if (parseResults.empty()) return false;

    // result from base class parsing of ASCII
    const Sample* psamp = parseResults.front();

    unsigned int nParsedVals = psamp->getDataLength();
    const float* pdata = (const float*) psamp->getConstVoidDataPtr();

    const float* pend = pdata + nParsedVals;

    // u,v,w,tc,diag
    float uvwtd[5];

    for (unsigned int i = 0; i < sizeof(uvwtd) / sizeof(uvwtd[0]); i++) {
        if (pdata < pend) uvwtd[i] = *pdata++;
        else uvwtd[i] = floatNAN;
    }

    // get diagnostic value from parsed sample
    float diagval = floatNAN;
    bool diagOK = false;

    if (_diagIndex >= 0 && (unsigned) _diagIndex < sizeof(uvwtd)/sizeof(uvwtd[0])) {
        diagval = uvwtd[_diagIndex];
        diagOK = !isnan(diagval) && diagval == 0.0;
    }

    if (getDespike()) {
        bool spikes[4] = {false,false,false,false};
        despike(samp->getTimeTag(),uvwtd,4,spikes);
    }

#ifdef HAVE_LIBGSL
    // apply shadow correction before correcting for unusual orientation
    transducerShadowCorrection(samp->getTimeTag(),uvwtd);
#endif

    applyOrientation(samp->getTimeTag(), uvwtd);

    offsetsTiltAndRotate(samp->getTimeTag(), uvwtd);

    // new sample
    SampleT<float>* wsamp = getSample<float>(_noutVals);

    // First fill the output sample with the parsed values.  The
    // CharacterSensor::process() method takes care of filling any unparsed
    // values in the output sample with nan.  This way any extra variables
    // beyond the first 5 standard sonic variables (u,v,w,tc,diag) will get
    // passed on to the output.  Any derived sonic variables
    // (ldiag,dir,spd) will overwrite the value at their respective index.
    memcpy(wsamp->getVoidDataPtr(), psamp->getConstVoidDataPtr(),
           std::min(psamp->getDataByteLength(),
                    wsamp->getDataByteLength()));

    // any defined time lag has been applied by SerialSensor
    wsamp->setTimeTag(psamp->getTimeTag());
    wsamp->setId(_sampleId);

    // finished with parsed sample
    psamp->freeReference();

    float* dout = wsamp->getDataPtr();

    float *dptr = dout;

    // Do not copy more than will fit into the output sample nor more than
    // exists in the uvwtd array.
    int nvals = ::min(_noutVals, (unsigned int)(sizeof(uvwtd)/sizeof(uvwtd[0])));

    memcpy(dptr, uvwtd, sizeof(float) * nvals);

#ifdef notdef
    // This is skipped now that the output sample is first seeded with all
    // the values from the parsed sample, otherwise it overwrites valid
    // parsed values.
    float* dend = dout + _noutVals;
    dptr += nvals;

    for ( ; dptr < dend; ) *dptr++ = floatNAN;
#endif

    // If user asks for ldiag, use it to flag data values
    if (_ldiagIndex >= 0) {
        dout[_ldiagIndex] = (float)!diagOK;
        if (!diagOK) {
            for (unsigned int i = 0; i < 4 && i < _noutVals; i++) {
                dout[i] = floatNAN;
            }
        }
    }

    if (_spdIndex >= 0) {
        dout[_spdIndex] = sqrt(dout[0] * dout[0] + dout[1] * dout[1]);
    }
    if (_dirIndex >= 0) {
        float dr = atan2f(-dout[0],-dout[1]) * 180.0 / M_PI;
        if (dr < 0.0) dr += 360.;
        dout[_dirIndex] = dr;
    }

    results.push_back(wsamp);
    return true;
}

WindRotator::WindRotator(): _angle(0.0),_sinAngle(0.0),_cosAngle(1.0) 
{
}

double WindRotator::getAngleDegrees() const
{
    return _angle * 180.0 / M_PI;
}

void WindRotator::setAngleDegrees(double val)
{
    _angle = val * M_PI / 180.0;
    _sinAngle = ::sin(_angle);
    _cosAngle = ::cos(_angle);
}

void WindRotator::rotate(float* up, float* vp) const
{
    float u = (float)( *up * _cosAngle + *vp * _sinAngle);
    float v = (float)(-*up * _sinAngle + *vp * _cosAngle);
    *up = u;
    *vp = v;
}

WindTilter::WindTilter(): _lean(0.0),_leanaz(0.0),_identity(true),
	UP_IS_SONIC_W(false)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
	    _mat[i][j] = (i == j ? 1.0 : 0.0);
}

void WindTilter::rotate(float* up, float* vp, float* wp) const
{

    if (_identity) return;

    float vin[3] = {*up,*vp,*wp};
    double out[3];

    for (int i = 0; i < 3; i++) {
	out[i] = 0.0;
	for (int j = 0; j < 3; j++)
	    out[i] += _mat[i][j] * vin[j];
    }
    *up = (float) out[0];
    *vp = (float) out[1];
    *wp = (float) out[2];
}

void WindTilter::computeMatrix()
{
    double sinlean,coslean,sinaz,cosaz;
    double mag;

    _identity = fabs(_lean) < 1.e-5;

    sinlean = ::sin(_lean);
    coslean = ::cos(_lean);
    sinaz = ::sin(_leanaz);
    cosaz = ::cos(_leanaz);

    /*
     *This is Wf, the flow W axis in the sonic UVW system.
     */
    _mat[2][0] = sinlean * cosaz;
    _mat[2][1] = sinlean * sinaz;
    _mat[2][2] = coslean;


    if (UP_IS_SONIC_W) {

      /* Uf is cross product of Vs (sonic V axis = 0,1,0) with Wf */
      mag = ::sqrt(coslean*coslean + sinlean*sinlean*cosaz*cosaz);

      _mat[0][0] = coslean / mag;
      _mat[0][1] = 0.0f;
      _mat[0][2] = -sinlean * cosaz / mag;
    }
    else {
      {
	double WfXUs[3];
        /* cross product of Wf and Us */
	WfXUs[0] = 0.0f;
	WfXUs[1] = coslean;
	WfXUs[2] = -sinlean * sinaz;

        /* Uf is cross of above with Wf */
	_mat[0][0] = WfXUs[1] * _mat[2][2] - WfXUs[2] * _mat[2][1];
	_mat[0][1] = WfXUs[2] * _mat[2][0] - WfXUs[0] * _mat[2][2];
	_mat[0][2] = WfXUs[0] * _mat[2][1] - WfXUs[1] * _mat[2][0];

	mag = ::sqrt(_mat[0][0]*_mat[0][0] + _mat[0][1]*_mat[0][1] + _mat[0][2]*_mat[0][2]);
	_mat[0][0] /= mag;
	_mat[0][1] /= mag;
	_mat[0][2] /= mag;
      }
    }

    /*  Vf = Wf cross Uf. */
    _mat[1][0] = _mat[2][1] * _mat[0][2] - _mat[2][2] * _mat[0][1];
    _mat[1][1] = _mat[2][2] * _mat[0][0] - _mat[2][0] * _mat[0][2];
    _mat[1][2] = _mat[2][0] * _mat[0][1] - _mat[2][1] * _mat[0][0];
}

