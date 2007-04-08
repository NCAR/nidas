/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-04 13:00:32 -0700 (Sun, 04 Mar 2007) $

    $LastChangedRevision: 3701 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_A2DSensor.h $

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_DSC_ANALOG_OUT_H
#define NIDAS_DYNLD_DSC_ANALOG_OUT_H

#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/core/DOMable.h>

#include <nidas/util/EndianConverter.h>
#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

#include <vector>

namespace nidas { namespace dynld {

/**
 * Support for the D2A device on a Diamond DMMAT card.
 */
class DSC_AnalogOut: public nidas::core::DOMable {

public:

    DSC_AnalogOut();

    ~DSC_AnalogOut();

    void setDeviceName(const std::string& val)
    {
        devName = val;
    }

    const std::string& getDeviceName() const
    {
        return devName;
    }

    const std::string& getName() const
    {
        return devName;
    }

    const int getFd() const { return fd; }

    /**
     * Open the D2A.
     */
    void open() throw(nidas::util::IOException);

    /**
     * Close the D2A.
     */
    void close() throw(nidas::util::IOException);

    /**
     * Return number of VOUT pins on this device.
     * Value will be 0 if the device has not been opened,
     * or -1 if the open failed.
     */
    int getNumOutputs() const 
    {
        return noutputs;
    }

    /**
     * Return the minimum setable voltage of an output.
     */
    float getMinVoltage(int i) const;

    /**
     * Return the maximum setable voltage of an output.
     */
    float getMaxVoltage(int i) const;

    /**
     * Set a voltage on an output.
     * @param which Which VOUT pin, numbered from 0.
     * @param val Desired voltage value. If val is outside
     *      the known limit of the D2A, the request will
     *      be changed to the corresponding limit value.
     * Throws nidas::util::InvalidParameterException if "which"
     * is out of range.
     */
    void setVoltage(int which,float val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    /**
     * Set one or more outputs.
     * @param which Vector of VOUT pin numbers, numbered from 0.
     * @param val Desired voltage values. If a val is outside
     *      the known limit of the D2A, the request will
     *      be changed to the corresponding limit value.
     * Throws nidas::util::InvalidParameterException if "which"
     * is out of range.
     */
    void setVoltages(const std::vector<int>& which,
        const std::vector<float>& val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

private:

    std::string devName;

    /**
     * The file descriptor of this device.
     */
    int fd;

    /**
     * How many VOUT pins are on this device?
     */
    int noutputs;

    /**
     * Linear parameters for converting a voltage to an integer count,
     * which is sent to the device.
     */
    struct DMMAT_D2A_Conversion conv;
};

}}	// namespace nidas namespace dynld

#endif
