/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef DSM_SAMPLETAG_H
#define DSM_SAMPLETAG_H

#include <DOMable.h>
#include <Variable.h>
#include <Sample.h>

#include <vector>
#include <list>
#include <algorithm>

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
    SampleTag():id(0),sampleId(0),sensorId(0),rate(0.0),processed(true) {}

    virtual ~SampleTag();

    /**
     * Set the 26 bit id, containing the DSM id and the sensor+sample id.
     * A sample tag ID is a 32-bit value comprised of four parts:
     * 6-bit type_id,  10-bit DSM_id,  16-bit sensor+sample id.
     * The 16-bit sensor+sample id is also know as the shortId.
     * The type id is not set-able here, it is only meaningful in an
     * actual Sample.
     */
    void setId(dsm_sample_id_t val) { id = SET_SAMPLE_ID(id,val); }

    /**
     * Set the sample portion of the shortId.
     */
    void setSampleId(unsigned short val) {
	sampleId = val;
        id = SET_SHORT_ID(id,sensorId + sampleId);
    }
    unsigned short getSampleId() const { return sampleId; }

    /**
     * Set the sensor portion of the shortId.
     */
    void setSensorId(unsigned short val) {
        sensorId = val;
    	id = SET_SHORT_ID(id,sensorId + sampleId);
    }
    unsigned short getSensorId() const { return sensorId; }

    /**
     * Set the DSM portion of the id.
     */
    void setDSMId(unsigned short val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the 26 bit id, containing the DSM id and the sensor+sample id.
     */
    dsm_sample_id_t getId()      const { return GET_SAMPLE_ID(id); }

    /**
     * Get the DSM portion of the id.
     */
    unsigned short  getDSMId() const { return GET_DSM_ID(id); }

    /**
     * Get the sensor+sample portion of the id.
     */
    unsigned short  getShortId() const { return GET_SHORT_ID(id); }

    /**
     * Suffix, which is added to variable names.
     */
    const std::string& getSuffix() const { return suffix; }

    void setSuffix(const std::string& val);

    /**
     * Set sampling rate in samples/sec.  Derived SampleTags can
     * override this method and throw an InvalidParameterException
     * if they can't support the rate value.  Sometimes
     * a rate of 0.0 may mean don't sample the variables in the
     * SampleTag.
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

    /**
     * Set if this sample is going to be post processed.
     */
    void setProcessed(bool val)
    	throw(atdUtil::InvalidParameterException)
    {
        processed = val;
    }
    /// Test to see if this sample is to be post processed.
    const bool isProcessed() const { return processed; };

    void setScanfFormat(const std::string& val)
    {
        scanfFormat = val;
    }

    const std::string& getScanfFormat() const { return scanfFormat; }

    /**
     * Add a variable to this SampleTag.  SampleTag
     * will own the Variable, and will delete
     * it in its destructor.
     */
    virtual void addVariable(Variable* var)
    	throw(atdUtil::InvalidParameterException);

    const std::vector<const Variable*>& getVariables() const;

    /**
     * What is the index of a Variable in this SampleTag.
     * @return -1: 'tain't here
     */
    int getIndex(const Variable* var) const
    {
	std::vector<const Variable*>::const_iterator vi =
	    std::find(constVariables.begin(),constVariables.end(),var);
        return (vi == constVariables.end() ? -1 : vi - constVariables.begin());
    }

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    dsm_sample_id_t id;

    unsigned short sampleId;

    unsigned short sensorId;

    std::string suffix;

    float rate;

    bool processed;

    std::vector<const Variable*> constVariables;

    std::vector<Variable*> variables;

    std::string scanfFormat;
};

}

#endif
