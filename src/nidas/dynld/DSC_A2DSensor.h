/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_DSC_A2DSENSOR_H
#define NIDAS_DYNLD_DSC_A2DSENSOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/dynld/A2DSensor.h>

#include <nidas/linux/diamond/dmd_mmat.h>
// #include <nidas/linux/filters/short_filters.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * One or more sensors connected to a Diamond Systems Corp A2D.
 */
class DSC_A2DSensor : public A2DSensor {

public:

    DSC_A2DSensor();
    ~DSC_A2DSensor();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    void addSampleTag(SampleTag* tag)
            throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

    int getMaxNumChannels() const { return MAX_DMMAT_A2D_CHANNELS; }

    void setA2DParameters(int ichan,int gain,int bipolar)
               throw(nidas::util::InvalidParameterException);

    void getBasicConversion(int ichan,float& intercept, float& slope) const;

private:

};

}}	// namespace nidas namespace dynld

#endif
