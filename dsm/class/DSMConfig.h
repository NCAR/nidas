/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_DSMCONFIG_H
#define DSM_DSMCONFIG_H

#include <DOMable.h>
#include <DSMSensor.h>
#include <SampleOutput.h>

#include <list>

namespace dsm {

class Site;
class PortSelector;

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

    void setSite(const Site* val) { site = val; }
    const Site* getSite() const { return site; }

    const std::string& getName() const { return name; }
    void setName(const std::string& val) { name = val; }

    const std::string& getLocation() const { return location; }
    void setLocation(const std::string& val) { location = val; }

    const unsigned short getId() const { return id; }
    void setId(unsigned short val) { id = val; }

    void addSensor(DSMSensor* sensor);
    const std::list<DSMSensor*>& getSensors() const { return sensors; }

    void openSensors(PortSelector*) throw(atdUtil::IOException);

    void addOutput(SampleOutput* output) { outputs.push_back(output); }
    const std::list<SampleOutput*>& getOutputs() const { return outputs; }

    unsigned short getRemoteSerialSocketPort() const { return remoteSerialSocketPort; }

    void setRemoteSerialSocketPort(unsigned short val) { remoteSerialSocketPort = val; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:

    const Site* site;

    std::string name;
    
    std::string location;

    unsigned char id;

    /**
     * The list of sensors on this DSM.
     */
    std::list<DSMSensor*> sensors;

    /**
     * Another list of the sensors on this DSM, but this one
     * contains the sensors that have not been passed to
     * a PortSelector, i.e. the sensors that DSMConfig still ownes.
     * On a running DSM this list will be empty.  On a DSMServer
     * it will contain all the sensors.
     */
    std::list<DSMSensor*> ownedSensors;

    /**
     * SampleOutputs.
     */
    std::list<SampleOutput*> outputs;

    /**
     * TCP socket port that DSMEngine listens on for remote serial
     * connections.  0=don't listen.
     */
    unsigned short remoteSerialSocketPort;	

};

}

#endif
