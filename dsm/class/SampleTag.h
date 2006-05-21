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
#include <NidsIterators.h>

#include <vector>
#include <list>
#include <algorithm>

namespace dsm {

class DSMConfig;

/**
 * Class describing a group of variables that are sampled and
 * handled together.
 *
 * A SampleTag has an integer ID. This is the same ID that is
 * associated with Sample objects, allowing software to map
 * between a data sample and the meta-data associated with it.
 * 
 * A SampleTag/Sample ID is a 32-bit value comprised of four parts:
 * 6-bit type_id,  10-bit DSM_id,  16-bit sensor+sample id.
 *
 * The type id specifies the data type (float, int, double, etc),
 * The type_id is only meaningful in an actual data Sample,
 * and is not accessible in the SampleTag class.
 * 
 * The 26 bits of DSM_id and sensor+sample are known simply as the
 * Id (or full id), and is accessible with the getId() method.
 *
 * The DSM_id contains the id of the data acquisition system that
 * collected the data, and can be accessed separately
 * from the other fields with getDSMId() and setDSMId().
 *
 * The 16-bit sensor+sample id is also known as the shortId.
 * To maintain flexibility, the shortId has not been divided
 * further into bit fields of sensor and sample id, but
 * is a sum of the two. This means that you cannot
 * set the shortId without losing track of the sensor and
 * sample ids.  For this reason, methods to set the shortId
 * and fullId are protected.
 *
 * To access the portions of the shortId, use getSensorId(),
 * setSensorId(), getSampleId() and setSampleId().
 *
 * Example: a DSMSensor has an id of 200, and four
 *    associated SampleTags with sample ids of 1,2,3 and 4.
 *    Therefore one should do a setSensorId(200) on each
 *    of the SampleTags, so that their shortIds become
 *    201,202,203, and 204. The convention is that processed
 *    samples have sample ids >= 1. Raw, unprocessed Samples from 
 *    this sensor have a sample id of 0, and therefore a shortId of 200.
 * 
 * A SampleTag also has a rate attribute, indicating the requested
 * sampling rate for the variables.
 */
class SampleTag : public DOMable
{

public:

    /**
     * Constructor.
     */
    SampleTag();

    /**
     * Copy constructor.
     */
    SampleTag(const SampleTag&);

    virtual ~SampleTag();

    /**
     * Set the sample portion of the shortId.
     */
    void setSampleId(unsigned long val) {
	sampleId = val;
        id = SET_SHORT_ID(id,sensorId + sampleId);
    }

    /**
     * Get the sample portion of the shortId.
     */
    unsigned long getSampleId() const { return sampleId; }

    /**
     * Set the sensor portion of the shortId.
     */
    void setSensorId(unsigned long val) {
        sensorId = val;
    	id = SET_SHORT_ID(id,sensorId + sampleId);
    }

    /**
     * Get the sensor portion of the shortId.
     */
    unsigned long getSensorId() const { return sensorId; }

    /**
     * Set the DSM portion of the id.
     */
    void setDSMId(unsigned long val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the DSM portion of the id.
     */
    unsigned long  getDSMId() const { return GET_DSM_ID(id); }

    /**
     * Get the 26 bit id, containing the DSM id and the sensor+sample id.
     */
    dsm_sample_id_t getId()      const { return GET_FULL_ID(id); }

    /**
     * Get the sensor+sample portion of the id.
     */
    unsigned long  getShortId() const { return GET_SHORT_ID(id); }

    /**
     * Suffix, which is appended to variable names.
     */
    const std::string& getSuffix() const { return suffix; }

    void setSuffix(const std::string& val);

    /**
     * Station number.
     */
    int getStation() const { return station; }

    void setStation(int val);

    const DSMConfig* getDSM() const { return dsm; }

    void setDSM(const DSMConfig* val) { dsm = val; }
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
     * Set sampling period (1/rate) in sec.
     * A value of 0.0 means an unknown period.
     */
    virtual void setPeriod(float val)
    	throw(atdUtil::InvalidParameterException)
    {
        rate = (val > 0.0) ? 1.0 / val : 0.0;
    }

    /**
     * Get sampling period (1/rate) in sec.
     * A value of 0.0 means an unknown rate.
     */
    virtual float getPeriod() const
    {

	return (rate > 0.0) ?  1.0 / rate : 0.0;
    }

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
     * Provide a reference to a variable - allowing one to modify it.
     */
    Variable& getVariable(int i) { return *variables[i]; }

    /**
     * Add a parameter to this SampleTag. SampleTag
     * will then own the pointer and will delete it
     * in its destructor.
     */
    void addParameter(Parameter* val)
    {
        parameters.push_back(val);
        constParameters.push_back(val);
    }

    const std::list<const Parameter*>& getParameters() const
    {
        return constParameters;
    }

    const Parameter* getParameter(const std::string& name) const;

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

    /**
     * Utility function to expand ${TOKEN} fields in a string.
     * Here in the SampleTag is a strange place for it,
     * but the fields may be associated with a SampleTag,
     * like ${DSM} to get the DSM name.
     */
    std::string expandString(const std::string& input) const;

    /**
     * Utility function to get the value of a token.
     */
    std::string getTokenValue(const std::string& token) const;

    VariableIterator getVariableIterator() const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    /**
     * Set the full id.  We don't make this public, because when
     * you use it you can't keep track of the sensor and sample
     * portions of the shortID.
     */
    void setId(dsm_sample_id_t val) { id = SET_FULL_ID(id,val); }

    /**
     * Set the sensor + sample portions of the id.
     * We don't make this public, because when you use it you
     * can't keep track of the sensor and sample portions of the
     * shortID.
     */
    void setShortId(unsigned long val) { id = SET_SHORT_ID(id,val); }

private:

    dsm_sample_id_t id;

    unsigned long sampleId;

    unsigned long sensorId;

    std::string suffix;

    int station;

    float rate;

    bool processed;

    const DSMConfig* dsm;

    std::vector<const Variable*> constVariables;

    std::vector<Variable*> variables;

    std::vector<std::string> variableNames;

    std::string scanfFormat;

    /**
     * List of pointers to Parameters.
     */
    std::list<Parameter*> parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> constParameters;

};

}

#endif
