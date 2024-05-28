// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_DSC_A2DSENSOR_H
#define NIDAS_DYNLD_DSC_A2DSENSOR_H

#include "A2DSensor.h"

#include <nidas/linux/diamond/dmd_mmat.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld {

using namespace nidas::core;

class DSC_AnalogOut;

/**
 * One or more sensors connected to a Diamond Systems Corp A2D.
 */
class DSC_A2DSensor : public A2DSensor {

public:

    DSC_A2DSensor();
    ~DSC_A2DSensor();

    /**
     * @throws nidas::util::IOException
     **/
    IODevice* buildIODevice();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    SampleScanner* buildSampleScanner();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    /**
     * Open the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    /*
     * Close the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    void printStatus(std::ostream& ostr) throw();

    int getMaxNumChannels() const { return MAX_DMMAT_A2D_CHANNELS; }

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void setA2DParameters(int ichan, int gain, int bipolar);

    void getBasicConversion(int ichan,float& intercept, float& slope) const;


    void executeXmlRpc(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw();

    void getA2DSetup(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw();

    void testVoltage(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw();

private:

    /**
     * Used for auto_cal, diagnostic voltages output.
     */
    DSC_AnalogOut *d2a;

    /**
     * Each card can only support one gain value.
     */
    int _gain;

    /**
     * Each card can only support one polarity.
     */
    bool _bipolar;

    /**
     * Channels to engage by auto_cal
     */
    int _calset;

    /**
     * Voltage set by auto_cal for diagnostics.
     */
    int _voltage;

    /** No copying */
    DSC_A2DSensor(const DSC_A2DSensor&);

    /** No assignment */
    DSC_A2DSensor& operator=(const DSC_A2DSensor&);
};

}}	// namespace nidas namespace dynld

#endif
