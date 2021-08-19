// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_DSMMESASENSOR_H
#define NIDAS_DYNLD_RAF_DSMMESASENSOR_H

#include <nidas/linux/mesa.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensors connected to the Mesa AnythingIO card.  Current programming is
 * for a PMS1D-260X, Pulse Counting, and the APN-232 Radar Altimeter.
 * Digital in/out coming soon.
 */
class DSMMesaSensor : public DSMSensor {

public:
    DSMMesaSensor();
    ~DSMMesaSensor();

    /**
     * @throws nidas::util::IOException
     **/
    IODevice *
    buildIODevice();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    SampleScanner* buildSampleScanner();

    /**
     * open the sensor and perform any intialization to the driver.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void
    open(int flags);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void
    fromDOMElement(const xercesc::DOMElement *);

    /**
     * @throw()
     **/
    bool
    process(const Sample * samp, std::list<const Sample *>& results);

private:
    /**
     * Download FPGA code from flash/disk to driver.
     *
     * @returns whether file was succesfully transmitted.
     * @throws nidas::util::IOException
     **/
    void
    sendFPGACodeToDriver();

    /**
     * Set up for processing the input file.
     *
     * @see sendFPGACodeToDriver()
     *
     * @throws nidas::util::IOException
     **/
    void
    selectfiletype(FILE * fp, const std::string& fname);

    struct radar_set radar_info;
    struct pms260x_set p260x_info;
    struct counters_set counter_info;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
