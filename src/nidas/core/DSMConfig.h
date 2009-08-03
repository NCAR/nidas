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

#include <nidas/core/DOMable.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/FileSet.h>
#include <nidas/util/Inet4SocketAddress.h>

#include <list>

namespace nidas { namespace core {

class Site;
class SensorHandler;

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

    const std::string& getName() const { return _name; }
    void setName(const std::string& val) { _name = val; }

    const std::string& getLocation() const { return _location; }
    void setLocation(const std::string& val) { _location = val; }

    const dsm_sample_id_t getId() const { return _id; }
    void setId(dsm_sample_id_t val) { _id = val; }

    void addSensor(DSMSensor* sensor);

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
     * in a string.  If curly brackets are not
     * used, then the TOKEN should be delimited by a '/', a '.' or
     * the end of string, e.g.:  xxx/yyy/$ZZZ.dat
     */
    std::string expandString(const std::string& input) const;

    /**
     * Utility function to get the value of a token.
     */
    std::string getTokenValue(const std::string& token) const;

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


protected:

    void removeSensor(DSMSensor* sensor);

private:

    const Site* _site;

    std::string _name;
    
    std::string _suffix;

    std::string _location;

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

    nidas::util::SocketAddress* _derivedDataSocketAddr;

    std::list<SampleIOProcessor*> _processors;

    nidas::util::SocketAddress* _statusSocketAddr;

};

}}	// namespace nidas namespace core

#endif
