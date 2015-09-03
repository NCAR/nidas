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
    _tcOffset(0.0),_tcSlope(1.0),
    _horizontalRotation(true),_tiltCorrection(true),
    _oaCalFile(0)
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

    // bias removal is part of the tilt correction.
    if (_tiltCorrection) {
        for (int i=0; i<3; i++) uvwt[i] -= _bias[i];
        if (!_tilter.isIdentity()) _tilter.rotate(uvwt,uvwt+1,uvwt+2);
    }
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

void SonicAnemometer::validate()
    throw(n_u::InvalidParameterException)
{

    DSMSerialSensor::validate();

    // Set default values of these parameters from the Project if they exist.
    // The value can be overridden with sensor parameters, below.
    const Parameter* parm =
        Project::getInstance()->getParameter("wind3d_horiz_rotation");
    if (parm) {
        if ((parm->getType() != Parameter::BOOL_PARAM &&
            parm->getType() != Parameter::INT_PARAM &&
            parm->getType() != Parameter::FLOAT_PARAM) ||
            parm->getLength() != 1)
            throw n_u::InvalidParameterException(getName(),parm->getName(),
                "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
        bool val = (bool) parm->getNumericValue(0);
        setDoHorizontalRotation(val);
    }

    parm = Project::getInstance()->getParameter("wind3d_tilt_correction");
    if (parm) {
        if ((parm->getType() != Parameter::BOOL_PARAM &&
            parm->getType() != Parameter::INT_PARAM &&
            parm->getType() != Parameter::FLOAT_PARAM) ||
            parm->getLength() != 1)
            throw n_u::InvalidParameterException(getName(),parm->getName(),
                "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
        bool val = (bool) parm->getNumericValue(0);
        setDoTiltCorrection(val);
    }

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    _allBiasesNaN = false;

    for ( ; pi != params.end(); ++pi) {
        parm = *pi;

        if (parm->getName() == "biases") {
            int nnan = 0;
            for (int i = 0; i < 3 && i < parm->getLength(); i++) {
                setBias(i,parm->getNumericValue(i));
                if (isnan(getBias(i))) nnan++;
            }
            if (nnan == 3) _allBiasesNaN = true;
        }
        else if (parm->getName() == "Vazimuth") {
            setVazimuth(parm->getNumericValue(0));
        }
        else if (parm->getName() == "despike") {
            setDespike(parm->getNumericValue(0) != 0.0);
        }
        else if (parm->getName() == "outlierProbability") {
            setOutlierProbability(parm->getNumericValue(0));
        }
        else if (parm->getName() == "discLevelMultiplier") {
            setDiscLevelMultiplier(parm->getNumericValue(0));
        }
        else if (parm->getName() == "lean") {
            setLeanDegrees(parm->getNumericValue(0));
        }
        else if (parm->getName() == "leanAzimuth") {
            setLeanAzimuthDegrees(parm->getNumericValue(0));
        }
        else if (parm->getName() == "wind3d_horiz_rotation") {
            if ((parm->getType() != Parameter::BOOL_PARAM &&
                parm->getType() != Parameter::INT_PARAM &&
                parm->getType() != Parameter::FLOAT_PARAM) ||
                parm->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),parm->getName(),
                    "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
            setDoHorizontalRotation((bool)parm->getNumericValue(0));
        }
        else if (parm->getName() == "wind3d_tilt_correction") {
            if ((parm->getType() != Parameter::BOOL_PARAM &&
                parm->getType() != Parameter::INT_PARAM &&
                parm->getType() != Parameter::FLOAT_PARAM) ||
                parm->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),parm->getName(),
                    "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
            setDoTiltCorrection((bool)parm->getNumericValue(0));
        }
        else if (parm->getName() == "orientation");
        else if (parm->getName() == "oversample");
        else if (parm->getName() == "soniclog");
        else if (parm->getName() == "shadowFactor");
        else if (parm->getName() == "maxShadowAngle");
        else if (parm->getName() == "expectedCounts");
        else if (parm->getName() == "maxMissingFraction");
        else if (parm->getName() == "bandwidth");
        else if (parm->getName() == "configure");
        else if (parm->getName() == "checkCounter");
        else throw n_u::InvalidParameterException(
             getName(),"parameter",parm->getName());
    }

    _oaCalFile =  getCalFile("offsets_angles");
    if (!_oaCalFile) _oaCalFile =  getCalFile("");
}
