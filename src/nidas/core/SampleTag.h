/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_CORE_SAMPLETAG_H
#define NIDAS_CORE_SAMPLETAG_H

#include <nidas/core/DOMable.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Sample.h>
#include <nidas/core/NidsIterators.h>

#include <vector>
#include <list>
#include <algorithm>

namespace nidas { namespace core {

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
    void setSampleId(unsigned int val) {
	sampleId = val;
        id = SET_SHORT_ID(id,sensorId + sampleId);
    }

    /**
     * Get the sample portion of the shortId.
     */
    unsigned int getSampleId() const { return sampleId; }

    /**
     * Set the sensor portion of the shortId.
     */
    void setSensorId(unsigned int val) {
        sensorId = val;
    	id = SET_SHORT_ID(id,sensorId + sampleId);
    }

    /**
     * Get the sensor portion of the shortId.
     */
    unsigned int getSensorId() const { return sensorId; }

    /**
     * Set the DSM portion of the id.
     */
    void setDSMId(unsigned int val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the DSM portion of the id.
     */
    unsigned int  getDSMId() const { return GET_DSM_ID(id); }

    /**
     * Get the 26 bit id, containing the DSM id and the sensor+sample id.
     */
    dsm_sample_id_t getId()      const { return GET_FULL_ID(id); }

    /**
     * Get the sensor+sample portion of the id.
     */
    unsigned int  getShortId() const { return GET_SHORT_ID(id); }

    /**
     * Suffix, which is appended to variable names.
     */
    const std::string& getSuffix() const { return suffix; }

    void setSuffix(const std::string& val);

    const DSMConfig* getDSM() const { return dsm; }

    void setDSM(const DSMConfig* val) { dsm = val; }

    /**
     * Figure out the Site of this SampleTag. Returns NULL if it
     * cannot be determined.
     */
    /**
     * Try to determine the associated site for this SampleTag.
     * A reference to the Site is not kept with the SampleTag.
     * Instead this method uses the station number, getStation(),
     * to find a site with the given number, or if that fails,
     * uses the dsm id found in the associated DSM.
     */
    const Site* getSite() const;

    /**
     * Set the site attributes of this SampleTag.
     * The Site pointer is not kept with the SampleTag.
     * The Site number is set on the SampleTag,
     * and if the Site number is 0, then the Site suffix
     * is appended to the Variable names.
     */
    void setSiteAttributes(const Site* val);

    /**
     * Station number.
     */
    int getStation() const { return station; }

    /**
     * Set sampling rate in samples/sec.  Derived SampleTags can
     * override this method and throw an InvalidParameterException
     * if they can't support the rate value.  Sometimes
     * a rate of 0.0 may mean don't sample the variables in the
     * SampleTag.
     */
    virtual void setRate(float val)
    	throw(nidas::util::InvalidParameterException)
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
    	throw(nidas::util::InvalidParameterException)
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
    	throw(nidas::util::InvalidParameterException)
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

    void setPromptString(const std::string& val)
    {
        _promptString = val;
    }

    const std::string& getPromptString() const { return _promptString; }

    /**
     * Add a variable to this SampleTag.  SampleTag
     * will own the Variable, and will delete
     * it in its destructor.
     */
    virtual void addVariable(Variable* var)
    	throw(nidas::util::InvalidParameterException);

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
     * What is the index of a Variable into the data of a sample from this SampleTag.
     * @return -1: 'tain't here
     */
    int getDataIndex(const Variable* var) const
    {
        int i = 0;
	std::vector<const Variable*>::const_iterator vi = constVariables.begin();
        for ( ; vi != constVariables.end(); ++vi) {
            if (*vi == var) return i;
            i += (*vi)->getLength();
        }
        return -1;
    }

    VariableIterator getVariableIterator() const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

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
    void setShortId(unsigned int val) { id = SET_SHORT_ID(id,val); }

private:

    dsm_sample_id_t id;

    unsigned int sampleId;

    unsigned int sensorId;

    std::string suffix;

    int station;

    float rate;

    bool processed;

    const DSMConfig* dsm;

    std::vector<const Variable*> constVariables;

    std::vector<Variable*> variables;

    std::vector<std::string> variableNames;

    std::string scanfFormat;

    std::string _promptString;

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

}}	// namespace nidas namespace core

#endif
