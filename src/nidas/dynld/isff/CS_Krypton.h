/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_CS_KRYPTON_H
#define NIDAS_DYNLD_ISFF_CS_KRYPTON_H

#include <nidas/core/VariableConverter.h>

#include <cmath>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * A class for making sense of data from a Campbell Scientific Inc
 * CSAT3 3D sonic anemometer.
 */
class CS_Krypton: public VariableConverter
{
public:

    CS_Krypton();

    CS_Krypton(const CS_Krypton&);

    ~CS_Krypton();

    CS_Krypton* clone() const;

    void setCalFile(CalFile* val);

    CalFile* getCalFile();

    /**
     * @param val Kw parameter from sensor calibration.
     */
    void setKw(float val)
    {
        Kw = val;
	pathLengthKw = pathLength * Kw;
    }

    float getKw() const
    {
        return Kw;
    }

    /**
     * @param val V0 value in millivolts.
     */
    void setV0(float val)
    {
        V0 = val;
	logV0 = ::log(V0);
    }

    float getV0() const
    {
        return V0;
    }

    /**
     * @param val Pathlength of sensor, in cm.
     */
    void setPathLength(float val)
    {
        pathLength = val;
	pathLengthKw = pathLength * Kw;
    }

    float getPathLength() const
    {
        return pathLength;
    }

    /**
     * @param val Bias (g/m^3) to be removed from data values.
     */
    void setBias(float val)
    {
        bias = val;
    }

    float getBias() const
    {
        return bias;
    }

    /**
     * Convert a voltage to water vapor density in g/m^3.
     */
    float convert(dsm_time_t t, float volts);

    std::string toString();

    void fromString(const std::string&) 
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

protected:

    float Kw;

    float V0;

    float logV0;

    float pathLength;

    float bias;

    float pathLengthKw;

    CalFile* calFile;

    dsm_time_t calTime;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
