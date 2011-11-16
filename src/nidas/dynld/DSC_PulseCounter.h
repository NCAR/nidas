// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_DSC_PULSECOUNTER_H
#define NIDAS_DYNLD_DSC_PULSECOUNTER_H

#include <nidas/core/DSMSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Sensor support for a simple pulse counter device.
 * This implementation supports a device that can
 * configured with a reporting period value ( e.g. 1 sec, 
 * or 1/10 sec).  Samples from the device are little-endian
 * 4 byte unsigned integer counts at the reporting period.
 * This class currently has hard-coded ioctl commands to
 * the dmd_mmat device driver which supports a counter on a
 * Diamond DMMAT card.
 */
class DSC_PulseCounter : public DSMSensor {

public:

    DSC_PulseCounter();
    ~DSC_PulseCounter();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    void init() throw(nidas::util::InvalidParameterException);
                                                                                
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means 
     * convert the input counts to a float.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

private:

    dsm_sample_id_t sampleId;

    int msecPeriod;

    const nidas::util::EndianConverter* cvtr;

    /** No copying. */
    DSC_PulseCounter(const DSC_PulseCounter&);

    /** No assignment. */
    DSC_PulseCounter& operator=(const DSC_PulseCounter&);

};

}}	// namespace nidas namespace dynld

#endif
