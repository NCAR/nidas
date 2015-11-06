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
#ifndef NIDAS_DYNLD_DSC_ANALOG_OUT_H
#define NIDAS_DYNLD_DSC_ANALOG_OUT_H

#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/util/EndianConverter.h>
#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

#include <vector>

namespace nidas { namespace dynld {

/**
 * Support for the D2A device on a Diamond DMMAT card.
 */
class DSC_AnalogOut {

public:

    DSC_AnalogOut();

    ~DSC_AnalogOut();

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
        return _noutputs;
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

private:

    std::string _devName;

    /**
     * The file descriptor of this device.
     */
    int _fd;

    /**
     * How many VOUT pins are on this device?
     */
    int _noutputs;

    /**
     * Linear parameters for converting a voltage to an integer count,
     * which is sent to the device.
     */
    struct DMMAT_D2A_Conversion _conv;
};

}}	// namespace nidas namespace dynld

#endif
