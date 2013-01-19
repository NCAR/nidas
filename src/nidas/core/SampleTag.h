// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/core/Sample.h>
#include <nidas/core/NidsIterators.h>

#include <vector>
#include <list>
#include <algorithm>

namespace nidas { namespace core {

class DSMConfig;
class Variable;
class Parameter;

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
     * Constructor of a sample for a given sensor.
     */
    SampleTag(const DSMSensor* sensor);

    /**
     * Copy constructor.
     */
    SampleTag(const SampleTag&);

    virtual ~SampleTag();

    SampleTag& operator=(const SampleTag& rhs);

    /**
     * Set the sample portion of the shortId.
     */
    void setSampleId(unsigned int val) {
	_sampleId = val;
        _id = SET_SPS_ID(_id,_sensorId + _sampleId);
    }

    /**
     * Get the sample portion of the shortId.
     */
    unsigned int getSampleId() const { return _sampleId; }

    /**
     * Set the sensor portion of the shortId.
     */
    void setSensorId(unsigned int val) {
        _sensorId = val;
    	_id = SET_SPS_ID(_id,_sensorId + _sampleId);
    }

    /**
     * Get the sensor portion of the shortId.
     */
    unsigned int getSensorId() const { return _sensorId; }

    /**
     * Set the DSM portion of the id.
     */
    void setDSMId(unsigned int val) { _id = SET_DSM_ID(_id,val); }

    /**
     * Get the DSM portion of the id.
     */
    unsigned int getDSMId() const { return GET_DSM_ID(_id); }

    /**
     * Get the 26 bit id, containing the DSM id and the sensor+sample id.
     */
    dsm_sample_id_t getId()      const { return GET_FULL_ID(_id); }

    /**
     * Get the sensor+sample portion of the id.
     */
    unsigned int getSpSId() const { return GET_SPS_ID(_id); }

    /**
     * Suffix, which is appended to variable names.
     */
    const std::string& getSuffix() const { return _suffix; }

    void setSuffix(const std::string& val);

    const DSMConfig* getDSMConfig() const { return _dsm; }

    void setDSMConfig(const DSMConfig* val) { _dsm = val; }

    const DSMSensor* getDSMSensor() const { return _sensor; }

    void setDSMSensor(const DSMSensor* val);

    /**
     * Station number, which is also known as the Site number. 
     * A station number of 0 is the "non" station.
     * Otherwise positive integers are used when a project
     * consists of more than one similar station, where
     * one can differentiate between the variables by
     * a station number (which maps to a station dimension
     * in a NetCDF file).
     * Setting the station on a SampleTag will set the:
     * the station on all its variables.
     */
    int getStation() const { return _station; }

    void setStation(int val);

    /**
     * Get the Site of this SampleTag, which will be non-NULL only
     * if getDSMConfig() returns non-NULL.
     */
    const Site* getSite() const;

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
        _rate = val;
    }

    /**
     * Get sampling rate in samples/sec.  A value of 0.0 means
     * an unknown rate.
     */
    virtual float getRate() const { return _rate; }

    /**
     * Set sampling period (1/rate) in sec.
     * A value of 0.0 means an unknown period.
     */
    virtual void setPeriod(float val)
    	throw(nidas::util::InvalidParameterException)
    {
        _rate = (val > 0.0) ? 1.0 / val : 0.0;
    }

    /**
     * Get sampling period (1/rate) in sec.
     * A value of 0.0 means an unknown rate.
     */
    virtual float getPeriod() const
    {

	return (_rate > 0.0) ?  1.0 / _rate : 0.0;
    }

    /**
     * Set if this sample is going to be post processed.
     */
    void setProcessed(bool val)
    	throw(nidas::util::InvalidParameterException)
    {
        _processed = val;
    }
    /// Test to see if this sample is to be post processed.
    bool isProcessed() const { return _processed; };

    void setScanfFormat(const std::string& val)
    {
        _scanfFormat = val;
    }

    const std::string& getScanfFormat() const { return _scanfFormat; }

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

    const std::vector<Variable*>& getVariables()
    {
        return _variables;
    }

    void removeVariable(const Variable* var);

    /**
     * Provide a reference to a variable - allowing one to modify it.
     */
    Variable& getVariable(int i) { return *_variables[i]; }

    /**
     * Add a parameter to this SampleTag. SampleTag
     * will then own the pointer and will delete it
     * in its destructor. If a Parameter already exists
     * with the same name, that Parameter will be removed
     * and deleted.
     */
    void addParameter(Parameter* val);

    const std::list<const Parameter*>& getParameters() const
    {
        return _constParameters;
    }

    const Parameter* getParameter(const std::string& name) const;

    /**
     * What is the index of a Variable into the data of a
     * sample from this SampleTag.
     * @return UINT_MAX: 'tain't here
     */
    unsigned int getDataIndex(const Variable* var) const;

    VariableIterator getVariableIterator() const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent,bool complete) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node,bool complete) const
    		throw(xercesc::DOMException);

protected:

    /**
     * Set the full id.  We don't make this public, because when
     * you use it you can't keep track of the sensor and sample
     * portions of the shortID.
     */
    void setId(dsm_sample_id_t val) { _id = SET_FULL_ID(_id,val); }

    /**
     * Set the sensor + sample portions of the id.
     * We don't make this public, because when you use it you
     * can't keep track of the sensor and sample portions of the
     * shortID.
     */
    void setSpSId(unsigned int val) { _id = SET_SPS_ID(_id,val); }

private:

    dsm_sample_id_t _id;

    unsigned int _sampleId;

    unsigned int _sensorId;

    std::string _suffix;

    int _station;

    float _rate;

    bool _processed;

    const DSMConfig* _dsm;

    const DSMSensor* _sensor;

    std::vector<const Variable*> _constVariables;

    std::vector<Variable*> _variables;

    std::vector<std::string> _variableNames;

    std::string _scanfFormat;

    std::string _promptString;

    /**
     * List of pointers to Parameters.
     */
    std::list<Parameter*> _parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> _constParameters;

};

}}	// namespace nidas namespace core

#endif
