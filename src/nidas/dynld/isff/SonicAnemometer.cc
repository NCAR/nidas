/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/SonicAnemometer.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

SonicAnemometer::SonicAnemometer():
    _allBiasesNaN(false),
    _despike(false),
    _rotator(),_tilter(),
    _calTime(0),_tcOffset(0.0),_tcSlope(1.0),
    _horizontalRotation(true),_tiltCorrection(true)
{
    for (int i = 0; i < 3; i++) {
	_bias[i] = 0.0;
    }
    for (int i = 0; i < 4; i++) {
	_ttlast[i] = 0;
    }
}

void SonicAnemometer::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);
}

void SonicAnemometer::despike(dsm_time_t tt,
	float* uvwt,int n,bool* spikeOrMissing) throw()
{
    vector<float> dvec(n);	// despiked data
    float* duvwt = &dvec.front();
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
    if (getDespike()) memcpy(uvwt,duvwt,n*sizeof(float));
}

void SonicAnemometer::offsetsTiltAndRotate(dsm_time_t tt,float* uvwt) throw()
{
    // Read CalFile of bias and rotation angles.
    // u.off   v.off   w.off    theta  phi    Vazimuth  t.off t.slope
    CalFile* cf = getCalFile();
    if (cf) {
        while(tt >= _calTime) {
            float d[8];
            try {
                int n = cf->readData(d,sizeof d/sizeof(d[0]));
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
                _calTime = cf->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < 3; i++) setBias(i,floatNAN);
                setLeanDegrees(floatNAN);
                setLeanAzimuthDegrees(floatNAN);
                setVazimuth(floatNAN);
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < 3; i++) setBias(i,floatNAN);
                setLeanDegrees(floatNAN);
                setLeanAzimuthDegrees(floatNAN);
                setVazimuth(floatNAN);
                _calTime = LONG_LONG_MAX;
            }
        }
    }
    for (int i=0; i<3; i++) uvwt[i] -= _bias[i];

    if (_tiltCorrection && !_tilter.isIdentity()) _tilter.rotate(uvwt,uvwt+1,uvwt+2);
    if (_horizontalRotation) _rotator.rotate(uvwt,uvwt+1);
}

WindRotator::WindRotator(): _angle(0.0),_sinAngle(0.0),_cosAngle(1.0) 
{
}

float WindRotator::getAngleDegrees() const
{
    return (float)(_angle * 180.0 / M_PI);
}

void WindRotator::setAngleDegrees(float val)
{
    _angle = (float)(val * M_PI / 180.0);
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
    float out[3];

    for (int i = 0; i < 3; i++) {
	out[i] = 0.0;
	for (int j = 0; j < 3; j++)
	    out[i] += (float)(_mat[i][j] * vin[j]);
    }
    *up = out[0];
    *vp = out[1];
    *wp = out[2];
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

void SonicAnemometer::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    DSMSerialSensor::fromDOMElement(node);

    // Set default values of these parameters from the Project if they exist.
    const Parameter* parm =
        Project::getInstance()->getParameter("wind3d_horiz_rotation");
    if (parm && (parm->getType() == Parameter::BOOL_PARAM ||
            parm->getType() == Parameter::INT_PARAM ||
            parm->getType() == Parameter::FLOAT_PARAM) &&
            parm->getLength() == 1) {
        bool val = (bool) parm->getNumericValue(0);
        setDoHorizontalRotation(val);
    }

    parm = Project::getInstance()->getParameter("wind3d_tilt_correction");
    if (parm && (parm->getType() == Parameter::BOOL_PARAM ||
            parm->getType() == Parameter::INT_PARAM ||
            parm->getType() == Parameter::FLOAT_PARAM) &&
            parm->getLength() == 1) {
        bool val = (bool) parm->getNumericValue(0);
        setDoTiltCorrection(val);
    }

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    _allBiasesNaN = false;

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

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
            if (parameter->getLength() != 1)
                throw n_u::InvalidParameterException(
                    getName(),parameter->getName(),"should be of length 1");
            setDoHorizontalRotation((bool)parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "wind3d_tilt_correction") {
            if (parameter->getLength() != 1)
                throw n_u::InvalidParameterException(
                    getName(),parameter->getName(),"should be of length 1");
            setDoTiltCorrection((bool)parameter->getNumericValue(0));
        }
        else if (parameter->getName() == "orientation");
        else if (parameter->getName() == "oversample");
        else if (parameter->getName() == "soniclog");
        else if (parameter->getName() == "shadowFactor");
        else if (parameter->getName() == "maxShadowAngle");
        else if (parameter->getName() == "expectedCounts");
        else if (parameter->getName() == "maxMissingFraction");
        else if (parameter->getName() == "bandwidth");
        else if (parameter->getName() == "configure");
        else if (parameter->getName() == "checkCounter");
        else throw n_u::InvalidParameterException(
             getName(),"parameter",parameter->getName());
    }

}
