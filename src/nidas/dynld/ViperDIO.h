/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_VIPER_DIO_H
#define NIDAS_DYNLD_VIPER_DIO_H

#include <nidas/linux/viper/viper_dio.h>

#include <nidas/core/DOMable.h>

#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/BitArray.h>

namespace nidas { namespace dynld {

/**
 * Support for the digital IO on an Arcom Viper.
 */
class ViperDIO: public nidas::core::DOMable {

public:

    ViperDIO();

    ~ViperDIO();

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
     * Open the DIO device.
     */
    void open() throw(nidas::util::IOException);

    /**
     * Close the DIO device.
     */
    void close() throw(nidas::util::IOException);

    /**
     * Return number of DOUT pins on this device.
     * Value will be 0 if the device has not been opened,
     * or -1 if the open failed.
     */
    int getNumOutputs() const 
    {
        return noutputs;
    }

    /**
     * Return number of DIN pins on this device.
     * Value will be 0 if the device has not been opened,
     * or -1 if the open failed.
     */
    int getNumInputs() const 
    {
        return ninputs;
    }

    void clearOutputs(const nidas::util::BitArray& which) 
        throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    void setOutputs(const nidas::util::BitArray& which) 
        throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    void setOutputs(const nidas::util::BitArray& which,
        const nidas::util::BitArray& val) 
        throw(nidas::util::IOException,
                nidas::util::InvalidParameterException);

    nidas::util::BitArray getOutputs() throw(nidas::util::IOException);

    nidas::util::BitArray getInputs() throw(nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

private:

    std::string devName;

    /**
     * The file descriptor of this device.
     */
    int fd;

    /**
     * How many DOUT pins are on this device?
     */
    int noutputs;

    /**
     * How many DIN pins are on this device?
     */
    int ninputs;

};

}}	// namespace nidas namespace dynld

#endif
