/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/
#ifndef DSM_SAMPLETAG_H
#define DSM_SAMPLETAG_H

#include <DOMable.h>
#include <Variable.h>

#include <vector>
#include <list>

namespace dsm {
/**
 * Class describing a sampled variable.
 */
class SampleTag : public DOMable
{

public:

    /**
     * Constructor.
     */
    SampleTag() {}

    virtual ~SampleTag();

    void setId(unsigned short val) { id = val; }

    unsigned short getId() const { return id; }

    /**
     * Set sampling rate in samples/sec.  A value of 0.0 means
     * an unknown rate. Derived sensors can override this method
     * and throw an InvalidParameterException if they can't support
     * the rate value.
     */
    virtual void setRate(float val)
    	throw(atdUtil::InvalidParameterException)
    {
        rate = val;
    }

    /**
     * Get sampling rate in samples/sec.  A value of 0.0 means
     * an unknown rate.
     */
    virtual float getRate() const { return rate; }

    virtual void addVariable(Variable* var)
    	throw(atdUtil::InvalidParameterException);

    const std::vector<const Variable*>& getVariables() const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    unsigned short id;

    float rate;

    std::vector<const Variable*> constVariables;

    std::list<Variable*> variables;
};

}

#endif
