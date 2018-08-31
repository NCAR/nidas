// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_VARIABLECONVERTER_H
#define NIDAS_CORE_VARIABLECONVERTER_H

#include "DOMable.h"
#include "Sample.h"
#include "Parameter.h"

#include <string>
#include <vector>

namespace nidas { namespace core {

class DSMConfig;
class DSMSensor;
class CalFile;
class Variable;

class VariableConverter: public DOMable
{
public:

    VariableConverter();

    /**
     * Copy constructor.
     */
    VariableConverter(const VariableConverter& x);

    /**
     * Assignment.
     */
    VariableConverter& operator=(const VariableConverter& x);

    virtual ~VariableConverter();

    virtual VariableConverter* clone() const = 0;

    virtual void readCalFile(dsm_time_t) throw();

    virtual double convert(dsm_time_t,double v) = 0;

    void setUnits(const std::string& val) { _units = val; }

    virtual const std::string& getUnits() const { return _units; }

    void setVariable(const Variable* val) { _variable = val; }

    const Variable* getVariable() const { return _variable; }

    /**
     * Reset the converter to invalid or default settings, such as after an
     * error occurs parsing a CalFile.
     **/
    virtual void reset() = 0;

    const DSMSensor* getDSMSensor() const;

    const DSMConfig* getDSMConfig() const;

    /**
     * Generate a string description of this VariableConverter.
     * May be used in meta-data, for example Netcdf comment.
     */
    virtual std::string toString() const = 0;

    virtual void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException) {
        throw nidas::util::InvalidParameterException(
            "fromString() not supported in this VariableConverter");
    }

    static VariableConverter* createVariableConverter(
    	XDOMElement& child);

    static VariableConverter* createFromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Add a parameter to this VariableConverter. VariableConverter
     * will then own the pointer and will delete it
     * in its destructor. If a Parameter exists with the
     * same name, it will be replaced with the new Parameter.
     */
    void addParameter(Parameter* val);

    /**
     * Get list of parameters.
     */
    const std::list<const Parameter*>& getParameters() const
    {
	return _constParameters;
    }

    /**
     * Fetch a parameter by name. Returns a NULL pointer if
     * no such parameter exists.
     */
    const Parameter* getParameter(const std::string& name) const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    void setCalFile(CalFile*);

    CalFile* getCalFile()
    {
        return _calFile;
    }

    const CalFile* getCalFile() const
    {
        return _calFile;
    }

private:

    std::string _units;

    /**
     * Map of parameters by name.
     */
    std::map<std::string,Parameter*> _parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> _constParameters;

    const Variable* _variable;

protected:
    
    /**
     * Parse the fields in the current CalFile record for the particular
     * settings and coefficients needed by this converter.
     **/
    virtual void parseFields(CalFile* cf) = 0;

    void abortCalFile(const std::string& what);

    CalFile* _calFile;
};

/**
 * Why isn't this a sublcass of Polynomial which sets MAX_NUM_COEFFS to 2?
 **/
class Linear: public VariableConverter
{
public:

    Linear();

    Linear* clone() const;

    void setSlope(float val) { _slope = val; }

    float getSlope() const { return _slope; }

    void setIntercept(float val) { _intercept = val; }

    float getIntercept() const { return _intercept; }

    double convert(dsm_time_t t, double val);

    void reset();

    void parseFields(CalFile* cf);

    std::string toString() const;

    void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

private:

    float _slope;

    float _intercept;

};

class Polynomial: public VariableConverter
{
public:

    Polynomial();

    Polynomial* clone() const;

    void setCoefficients(const std::vector<float>& vals);

    void setCoefficients(const float* fp, unsigned int n);

    const std::vector<float>& getCoefficients() const { return _coefs; }

    const float* getCoefficients(unsigned int & n) const
    {
        n = _coefs.size();
        return &_coefs[0];
    }

    double convert(dsm_time_t t,double val);

    void reset();

    void parseFields(CalFile* cf);

    std::string toString() const;

    void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    /**
     * This is static and defined inline below so the implementation can be
     * shared with at least the one class in dynld which uses it:
     * ParoSci_202BG_Calibration.
     **/
    static double eval(double x,float *p, unsigned int np);

private:

    std::vector<float> _coefs;

    /**
     *  Maximum number of coefficients that can be read
     *  from a CalFile.
     */
    static const int MAX_NUM_COEFS = 6;

};

/* static */
inline double Polynomial::eval(double x,float *p, unsigned int np)
{
    double y = 0.0;
    if (np == 0) return y;
    for (unsigned int i = np - 1; i > 0; i--) {
        y += p[i];
        y *= x;
    }
    y += p[0];
    return y;
}

}}	// namespace nidas namespace core

#endif
