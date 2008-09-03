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
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

SonicAnemometer::SonicAnemometer(): counter(-1),despike(false),calTime(0),
    _tcOffset(0.0),_tcSlope(1.0)
{
    for (int i = 0; i < 3; i++) {
	bias[i] = 0.0;
    }
    for (int i = 0; i < 4; i++) {
	ttlast[i] = 0;
    }
}

void SonicAnemometer::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);
}

float SonicAnemometer::correctTcForPathCurvature(float tc,
	float u, float v, float w)
{
    return tc;
}

void SonicAnemometer::processSonicData(dsm_time_t tt,
	float* uvwt, float* spd, float* dir, float* flags) throw()
{

    if (despike || flags != 0) {
	bool spikeOrMissing[4];
	float duvwt[4];	// despiked data

	/*
	 * Despike data
	 */
	for (int i=0; i < 4; i++) {
	    /* Restart statistics after data gap. */
	    if (tt - ttlast[i] > DATA_GAP_USEC) despiker[i].reset();

	    /* Despike status, 1=despiked, 0=not despiked */
	    spikeOrMissing[i] = isnan(uvwt[i]);
	    if (i < 3) 
		duvwt[i] = despiker[i].despike(uvwt[i],spikeOrMissing+i);
	    else {
		/* 4th value is virtual temperature from the speed of sound.
		 * Use the despiker to forecast a temperature if the current
		 * data value is missing.
		 */
		spikeOrMissing[i] = spikeOrMissing[0] || spikeOrMissing[1] ||
		    spikeOrMissing[2] || spikeOrMissing[3];;
		if (spikeOrMissing[i]) 
		    duvwt[i] = despiker[i].despike(floatNAN,spikeOrMissing+i);
		else {
		    despiker[i].despike(uvwt[i],spikeOrMissing+i);
		    duvwt[i] = uvwt[i];
		}
	    }
	    if (flags) flags[i] = spikeOrMissing[i];
	    if (!spikeOrMissing[i]) ttlast[i] = tt;
	}

	if (despike) memcpy(uvwt,duvwt,4*sizeof(float));
    }

    // Read CalFile of bias and rotation angles.
    // u.off   v.off   w.off    theta  phi    Vazimuth  
    CalFile* cf = getCalFile();
    if (cf) {
        while(tt >= calTime) {
            float d[8];
            try {
                int n = cf->readData(d,sizeof d/sizeof(d[0]));
                for (int i = 0; i < 3 && i < n; i++) setBias(i,d[i]);
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
                calTime = cf->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < 3; i++) setBias(i,d[i]);
                setLeanDegrees(floatNAN);
                setLeanAzimuthDegrees(floatNAN);
                setVazimuth(floatNAN);
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                for (int i = 0; i < 3; i++) setBias(i,floatNAN);
                setLeanDegrees(floatNAN);
                setLeanAzimuthDegrees(floatNAN);
                setVazimuth(floatNAN);
                calTime = LONG_LONG_MAX;
            }
        }
    }
    for (int i=0; i<3; i++)
	if (!isnan(uvwt[i])) uvwt[i] -= bias[i];

    uvwt[3] = correctTcForPathCurvature(uvwt[3],uvwt[0],uvwt[1],uvwt[2]) * _tcSlope + _tcOffset;

    if (!tilter.isIdentity()) tilter.rotate(uvwt,uvwt+1,uvwt+2);
    rotator.rotate(uvwt,uvwt+1);

    if (spd != 0) *spd = sqrt(uvwt[0] * uvwt[0] + uvwt[1] * uvwt[1]);
    if (dir != 0) {
	float dr = atan2f(-uvwt[0],-uvwt[1]) * 180.0 / M_PI;
	if (dr < 0.0) dr += 360.;
	*dir = dr;
    }
}

WindRotator::WindRotator(): angle(0.0),sinAngle(0.0),cosAngle(1.0) 
{
}

float WindRotator::getAngleDegrees() const
{
    return (float)(angle * 180.0 / M_PI);
}

void WindRotator::setAngleDegrees(float val)
{
    angle = (float)(val * M_PI / 180.0);
    sinAngle = ::sin(angle);
    cosAngle = ::cos(angle);
}

void WindRotator::rotate(float* up, float* vp) const
{
    float u = (float)( *up * cosAngle + *vp * sinAngle);
    float v = (float)(-*up * sinAngle + *vp * cosAngle);
    *up = u;
    *vp = v;
}

WindTilter::WindTilter(): lean(0.0),leanaz(0.0),identity(true),
	UP_IS_SONIC_W(false)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
	    mat[i][j] = (i == j ? 1.0 : 0.0);
}


void WindTilter::rotate(float* up, float* vp, float* wp) const
{

    if (identity) return;

    float vin[3] = {*up,*vp,*wp};
    float out[3];

    for (int i = 0; i < 3; i++) {
	out[i] = 0.0;
	for (int j = 0; j < 3; j++)
	    out[i] += (float)(mat[i][j] * vin[j]);
    }
    *up = out[0];
    *vp = out[1];
    *wp = out[2];
}


void WindTilter::computeMatrix()
{
    double sinlean,coslean,sinaz,cosaz;
    double mag;

    identity = fabs(lean) < 1.e-5;

    sinlean = ::sin(lean);
    coslean = ::cos(lean);
    sinaz = ::sin(leanaz);
    cosaz = ::cos(leanaz);

    /*
     *This is Wf, the flow W axis in the sonic UVW system.
     */

    mat[2][0] = sinlean * cosaz;
    mat[2][1] = sinlean * sinaz;
    mat[2][2] = coslean;


    if (UP_IS_SONIC_W) {

      mag = ::sqrt(coslean*coslean + sinlean*sinlean*cosaz*cosaz);

      mat[0][0] = coslean / mag;
      mat[0][1] = 0.0f;
      mat[0][2] = -sinlean * cosaz / mag;
    }
    else {
      {
	double WfXUs[3];
	WfXUs[0] = 0.0f;
	WfXUs[1] = coslean;
	WfXUs[2] = -sinlean * sinaz;

	mat[0][0] = WfXUs[1] * mat[2][2] - WfXUs[2] * mat[2][1];
	mat[0][1] = WfXUs[2] * mat[2][0] - WfXUs[0] * mat[2][2];
	mat[0][2] = WfXUs[0] * mat[2][1] - WfXUs[1] * mat[2][0];

	mag = ::sqrt(mat[0][0]*mat[0][0] + mat[0][1]*mat[0][1] + mat[0][2]*mat[0][2]);
	mat[0][0] /= mag;
	mat[0][1] /= mag;
	mat[0][2] /= mag;
      }
    }

    /*  Vf = Wf cross Uf. */
    mat[1][0] = mat[2][1] * mat[0][2] - mat[2][2] * mat[0][1];
    mat[1][1] = mat[2][2] * mat[0][0] - mat[2][0] * mat[0][2];
    mat[1][2] = mat[2][0] * mat[0][1] - mat[2][1] * mat[0][0];
}

void SonicAnemometer::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    DSMSerialSensor::fromDOMElement(node);

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "biases") {
            for (int i = 0; i < 3 && i < parameter->getLength(); i++)
                    setBias(i,parameter->getNumericValue(i));
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
        else if (parameter->getName() == "orientation");
        else throw n_u::InvalidParameterException(
             getName(),"parameter",parameter->getName());
    }
}
