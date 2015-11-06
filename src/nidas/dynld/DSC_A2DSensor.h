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
