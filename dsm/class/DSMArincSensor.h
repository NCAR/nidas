/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

 ******************************************************************
*/
#ifndef DSMARINCSENSOR_H
#define DSMARINCSENSOR_H

#include <RTL_DSMSensor.h>
#include <atdUtil/InvalidParameterException.h>

namespace dsm {
/**
 * A sensor connected to an ARINC port.
 */
class DSMArincSensor : public RTL_DSMSensor {

public:

    DSMArincSensor();

    DSMArincSensor(const std::string& name);

    ~DSMArincSensor();

    /**
     * This opens the associated RT-Linux FIFOs.
     */
    void open(int flags) throw(atdUtil::IOException);

    /**
     * This closes the associated RT-Linux FIFOs.
     */
    void close() throw(atdUtil::IOException);

    /**
     * Extract the ARINC configuration elements from the XML header.
     */
    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

 private:

    /**
     * Rates for each label.
     */
    unsigned char labelRate[0400];
};

}

#endif
