/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_DSMCONFIG_H
#define DSM_DSMCONFIG_H

#include <DOMable.h>
#include <DSMSensor.h>
#include <SampleOutput.h>

#include <list>

namespace dsm {

class Aircraft;
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

    void setAircraft(const Aircraft* val) { aircraft = val; }
    const Aircraft* getAircraft() const { return aircraft; }

    const std::string& getName() const { return name; }
    void setName(const std::string& val) { name = val; }

    const std::string& getLocation() const { return location; }
    void setLocation(const std::string& val) { location = val; }

    const unsigned char getId() const { return id; }
    void setId(unsigned char val) { id = val; }

    void addSensor(DSMSensor* sensor);
    const std::list<DSMSensor*>& getSensors() const { return sensors; }

    void openSensors(PortSelector*) throw(atdUtil::IOException);

    void addRawOutput(SampleOutput* output) { rawOutputs.push_back(output); }
    const std::list<SampleOutput*>& getRawOutputs() const { return rawOutputs; }

    void addOutput(SampleOutput* output) { outputs.push_back(output); }
    const std::list<SampleOutput*>& getOutputs() const { return outputs; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:

    const Aircraft* aircraft;

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
     * SampleOutputs that are raw sample clients of sensors.
     */
    std::list<SampleOutput*> rawOutputs;

    /**
     * SampleOutputs that are normal sample clients of sensors.
     */
    std::list<SampleOutput*> outputs;

};

}

#endif
