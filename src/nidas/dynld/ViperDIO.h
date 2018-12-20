// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
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
 * There are 8 independent inputs, IN0-7, and 8 independent outputs
 * OUT0-7.  The value of the inputs can be read, and the value of
 * the outputs written or read.
 */
class ViperDIO {

public:

    ViperDIO();

    ~ViperDIO();

    void setDeviceName(const std::string& val)
    {
        _devName = val;
    }

    const std::string& getDeviceName() const
    {
        return _devName;
    }

    const std::string& getName() const
    {
        return _devName;
    }

    int getFd() const { return _fd; }

    /**
     * Open the DIO device.
     *
     * @throws nidas::util::IOException
     **/
    void open();

    /**
     * Close the DIO device.
     *
     * @throws nidas::util::IOException
     **/
    void close();

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
     * Clear, to low state, viper digital output ports OUT0-7,
     * as selected by bits 0-7 of which.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void clearOutputs(const nidas::util::BitArray& which);

    /**
     * Set, to high state, viper digital output ports OUT0-7,
     * as selected by bits 0-7 of which
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void setOutputs(const nidas::util::BitArray& which);

    /**
     * Set ports OUT0-7, selected by bits 0-7 of which,
     * to 0(low) or high(1) based on bits 0-7 of val
     *
     * @throws nidas::util::IOException,
     * @throws nidas::util::InvalidParameterException
     **/
    void setOutputs(const nidas::util::BitArray& which,
                    const nidas::util::BitArray& val);

    /**
     * Get current settings of OUT0-7
     *
     * @throws nidas::util::IOException
     **/
    nidas::util::BitArray getOutputs();

    /**
     * get current settings of IN0-7
     *
     * @throws nidas::util::IOException
     **/
    nidas::util::BitArray getInputs();

private:

    std::string _devName;

    /**
     * The file descriptor of this device.
     */
    int _fd;

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
