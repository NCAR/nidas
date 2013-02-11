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
#ifndef NIDAS_DYNLD_IR104_RELAYS_H
#define NIDAS_DYNLD_IR104_RELAYS_H

#include <nidas/linux/diamond/ir104.h>

#include <nidas/core/DSMSensor.h>

#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/BitArray.h>

namespace nidas { namespace dynld {

/**
 * Support for the digital IO on a Diamond Systems IR104 board.
 * There are 20 independent opto-isolated inputs, and 20 independent
 * relay outputs.  The value of the inputs can be read, and the value of
 * the outputs written or read.
 */
class IR104_Relays: public nidas::core::DSMSensor {

public:

    IR104_Relays();

    ~IR104_Relays();

    nidas::core::IODevice* buildIODevice() throw(nidas::util::IOException);

    nidas::core::SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the IR104 device.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /**
     * Process a raw sample of the relay bit settings.
     */
    bool process(const nidas::core::Sample*,std::list<const nidas::core::Sample*>& result)
                throw();

    /**
     * Return number of DOUT pins on this device (8).
     * Value will be 0 if the device has not been opened,
     * or -1 if the open failed.
     */
    int getNumOutputs() const 
    {
        return _noutputs;
    }

    /**
     * Return number of DIN pins on this device (8).
     * Value will be 0 if the device has not been opened,
     * or -1 if the open failed.
     */
    int getNumInputs() const 
    {
        return _ninputs;
    }

    /**
     * Unset relays as selected by bits 0-19 of which.
     */
    void clearOutputs(const nidas::util::BitArray& which) 
        throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    /**
     * Set relays as selected by bits 0-19 of which.
     */
    void setOutputs(const nidas::util::BitArray& which) 
        throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    /**
     * Set relays, selected by bits 0-19 of which,
     * to 0(low) or high(1) based on bits 0-19 of val
     */
    void setOutputs(const nidas::util::BitArray& which,
        const nidas::util::BitArray& val) 
        throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    /**
     * Get current settings of relays.
     */ 
    nidas::util::BitArray getOutputs() throw(nidas::util::IOException);

    /**
     * get current settings of inputs.
     */
    nidas::util::BitArray getInputs() throw(nidas::util::IOException);

private:

    /**
     * How many DOUT pins are on this device?
     */
    int _noutputs;

    /**
     * How many DIN pins are on this device?
     */
    int _ninputs;

};

}}	// namespace nidas namespace dynld

#endif
