/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_VARIABLECONVERTER_H
#define DSM_VARIABLECONVERTER_H

#include <DOMable.h>

#include <string>
#include <vector>

namespace dsm {

class VariableConverter: public DOMable
{
public:

    virtual ~VariableConverter() {}

    virtual VariableConverter* clone() const = 0;

    virtual float convert(float) const = 0;

    void setUnits(const std::string& val) { units = val; }
    virtual const std::string& getUnits() const { return units; }

    virtual std::string toString() const = 0;

    virtual void fromString(const std::string&)
    	throw(atdUtil::InvalidParameterException) = 0;

    static VariableConverter* createVariableConverter(
    	const std::string& elname);

    static VariableConverter* createFromString(const std::string&)
    	throw(atdUtil::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


protected:

    std::string units;

};

class Linear: public VariableConverter
{
public:

    Linear* clone() const { return new Linear(*this); }

    void setSlope(float val) { slope = val; }

    float getSlope() const { return slope; }

    void setIntercept(float val) { intercept = val; }

    float getIntercept() const { return intercept; }

    float convert(float val) const { return val * slope + intercept; }

    std::string toString() const;

    void fromString(const std::string&)
    	throw(atdUtil::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

private:
    float slope;
    float intercept;
};

class Polynomial: public VariableConverter
{
public:

    Polynomial() : coefs(0) {}

    /**
     * Copy constructor.
     */
    Polynomial(const Polynomial&);

    ~Polynomial() { delete [] coefs; }

    Polynomial* clone() const { return new Polynomial(*this); }

    void setCoefficients(const std::vector<float>& vals);

    const std::vector<float>& getCoefficients() const { return coefvec; }

    inline float convert(float val) const
    {
	double result = 0.0;
	for (int i = ncoefs - 1; i > 0; i--) {
	    result += coefs[i];
	    result *= val;
	}
	result += coefs[0];
	return result;
    }

    std::string toString() const;

    void fromString(const std::string&)
    	throw(atdUtil::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

private:
    std::vector<float> coefvec;

    float* coefs;

    unsigned int ncoefs;

};

}

#endif
