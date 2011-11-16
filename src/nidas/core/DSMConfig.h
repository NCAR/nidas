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

#ifndef NIDAS_CORE_DSMCONFIG_H
#define NIDAS_CORE_DSMCONFIG_H

#include <nidas/core/Sample.h>
#include <nidas/core/DOMable.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/Dictionary.h>

#include <nidas/util/SocketAddress.h>

#include <list>

namespace nidas { namespace core {

class Project;
class Site;
class DSMSensor;
class FileSet;
class SensorHandler;
class SampleOutput;

/**
 * Class that should include all that is configurable about a
 * DSM.  It should be able to initialize itself from a
 * <dsm> XML element, and provide get methods to access
 * its essential pieces, like sensors.
 */
class DSMConfig : public DOMable {
public:
    DSMConfig();
    virtual ~DSMConfig();

    void setSite(const Site* val) { _site = val; }
    const Site* getSite() const { return _site; }

    const Project* getProject() const;

    const std::string& getName() const { return _name; }
    void setName(const std::string& val) { _name = val; }

    const std::string& getLocation() const { return _location; }
    void setLocation(const std::string& val) { _location = val; }

    dsm_sample_id_t getId() const { return _id; }
    void setId(dsm_sample_id_t val) { _id = val; }

    void addSensor(DSMSensor* sensor);
    void removeSensor(DSMSensor* sensor);

    const std::list<DSMSensor*>& getSensors() const
    {
	return _allSensors;
    }

    void initSensors() throw(nidas::util::IOException);

    /**
     * Pass my sensors to the SensorHandler for opening.
     */
    void openSensors(SensorHandler*);

    void addOutput(SampleOutput* output) { _outputs.push_back(output); }

    const std::list<SampleOutput*>& getOutputs() const { return _outputs; }

    std::list<nidas::core::FileSet*> findSampleOutputStreamFileSets() const;

    unsigned short getRemoteSerialSocketPort() const { return _remoteSerialSocketPort; }

    void setRemoteSerialSocketPort(unsigned short val) { _remoteSerialSocketPort = val; }

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    /**
     * Get the length of the SampleSorter of raw Samples, in seconds.
     */
    float getRawSorterLength() const
    {
        return _rawSorterLength;
    }

    /**
     * Set the length of the SampleSorter of raw Samples, in seconds.
     */
    void setRawSorterLength(float val)
    {
        _rawSorterLength = val;
    }

    /**
     * Get the length of the SampleSorter of processed Samples, in seconds.
     */
    float getProcSorterLength() const
    {
        return _procSorterLength;
    }

    /**
     * Set the length of the SampleSorter of processed Samples, in seconds.
     */
    void setProcSorterLength(float val)
    {
        _procSorterLength = val;
    }

    /**
     * Get the size of in bytes of the raw SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    size_t getRawHeapMax() const
    {
        return _rawHeapMax;
    }

    /**
     * Set the size of in bytes of the raw SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    void setRawHeapMax(size_t val)
    {
        _rawHeapMax = val;
    }

    /**
     * Get the size of in bytes of the processed SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    size_t getProcHeapMax() const
    {
        return _procHeapMax;
    }

    /**
     * Set the size of in bytes of the processed SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    void setProcHeapMax(size_t val)
    {
        _procHeapMax = val;
    }

    /**
     * Get the size of the late sample cache in the raw sample sorter.
     * See SampleSorter::getLateSampleCacheSize(). Default: 0.
     */
    unsigned int getRawLateSampleCacheSize() const
    {
        return _rawLateSampleCacheSize;
    }

    /**
     * Cache this number of samples with potentially anomalous, late time tags
     * in the raw sample sorter.
     * See SampleSorter::setLateSampleCacheSize(val).
     */
    void setRawLateSampleCacheSize(unsigned int val)
    {
        _rawLateSampleCacheSize = val;
    }

    /**
     * Get the size of the late sample cache in the processed sample sorter.
     * See SampleSorter::getLateSampleCacheSize(). Default: 0.
     */
    unsigned int getProcLateSampleCacheSize() const
    {
        return _procLateSampleCacheSize;
    }

    /**
     * Cache this number of samples with potentially anomalous, late time tags
     * in the processed sample sorter.
     * See SampleSorter::setLateSampleCacheSize(val).
     */
    void setProcLateSampleCacheSize(unsigned int val)
    {
        _procLateSampleCacheSize = val;
    }

    /**
     * Parse a DOMElement for a DSMSensor, returning a pointer to
     * the DSMSensor. The pointer may be for a new instance of a DSMSensor,
     * or, if the devicename matches a previous DSMSensor that has
     * been added to this DSMConfig, will point to the matching
     * DSMSensor.
     */
    DSMSensor* sensorFromDOMElement(const xercesc::DOMElement* node)
        throw(nidas::util::InvalidParameterException);

    void validate()
	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent,bool complete) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node,bool complete) const
    		throw(xercesc::DOMException);

    /**
     * Utility function to expand ${TOKEN} or $TOKEN fields
     * in a string with their value from getTokenValue().
     * If curly brackets are not used, then the TOKEN should
     * be delimited by a '/', a '.' or the end of string,
     * e.g.:  xxx/yyy/$ZZZ.dat
     */
    std::string expandString(const std::string& input) const
    {
        return _dictionary.expandString(input);
    }

    /**
     * Implement a lookup for tokens that I know about, like $DSM, $LOCATION.
     * For other tokens, call getSite()->getTokenValue(token,value);
     */
    bool getTokenValue(const std::string& token,std::string& value) const
    {
        return _dictionary.getTokenValue(token,value);
    }

    void setDerivedDataSocketAddr(const nidas::util::SocketAddress& val)
    {
        delete _derivedDataSocketAddr;
        _derivedDataSocketAddr = val.clone();
    }

    const nidas::util::SocketAddress& getDerivedDataSocketAddr() const
    {
        return *_derivedDataSocketAddr;
    }

    void setStatusSocketAddr(const nidas::util::SocketAddress& val)
    {
        delete _statusSocketAddr;
        _statusSocketAddr = val.clone();
    }

    const nidas::util::SocketAddress& getStatusSocketAddr() const
    {
        return *_statusSocketAddr;
    }

    /**
     * Add a processor to this DSM. This is done
     * at configuration (XML) time.
     */
    virtual void addProcessor(SampleIOProcessor* proc)
    {
        _processors.push_back(proc);
    }

    virtual const std::list<SampleIOProcessor*>& getProcessors() const
    {
        return _processors;
    }

    ProcessorIterator getProcessorIterator() const;

private:

    /**
     * Validate the ids of the DSMSensors belonging to this DSMConfig,
     * and their SampleTags, for uniqueness.
     */
    void validateSensorAndSampleIds()
	throw(nidas::util::InvalidParameterException);

    const Site* _site;

    std::string _name;
    
    std::string _suffix;

    std::string _location;

    class MyDictionary : public Dictionary {
    public:
        MyDictionary(DSMConfig* dsm): _dsm(dsm) {}
        MyDictionary(const MyDictionary& x): _dsm(x._dsm) {}
        MyDictionary& operator=(const MyDictionary& rhs)
        {
            if (&rhs != this) {
                _dsm = rhs._dsm;
            }
            return *this;
        }
        bool getTokenValue(const std::string& token, std::string& value) const;
    private:
        DSMConfig* _dsm;
    } _dictionary;

    unsigned char _id;

    /**
     * A list of the sensors on this DSM that have not been passed to
     * a SensorHandler, i.e. the sensors that DSMConfig still ownes.
     * On a running DSM this list will be empty.
     */
    std::list<DSMSensor*> _ownedSensors;

    /**
     * A list of all sensors configured on this DSM.
     */
    std::list<DSMSensor*> _allSensors;

    /**
     * SampleOutputs.
     */
    std::list<SampleOutput*> _outputs;

    /**
     * TCP socket port that DSMEngine listens on for remote serial
     * connections.  0=don't listen.
     */
    unsigned short _remoteSerialSocketPort;	

    float _rawSorterLength;

    float _procSorterLength;

    size_t _rawHeapMax;

    size_t _procHeapMax;

    unsigned int _rawLateSampleCacheSize;

    unsigned int _procLateSampleCacheSize;

    nidas::util::SocketAddress* _derivedDataSocketAddr;

    std::list<SampleIOProcessor*> _processors;

    nidas::util::SocketAddress* _statusSocketAddr;

private:
    // no copying
    DSMConfig(const DSMConfig& x);

    // no assignment
    DSMConfig& operator=(const DSMConfig& x);
};

}}	// namespace nidas namespace core

#endif
