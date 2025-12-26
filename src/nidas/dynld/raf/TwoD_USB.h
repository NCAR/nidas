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

#ifndef _nidas_dynld_raf_2d_usb_h_
#define _nidas_dynld_raf_2d_usb_h_

#include <nidas/core/DSMSensor.h>
#include <nidas/dynld/raf/TwoD_Processing.h>
#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/EndianConverter.h>
#include <nidas/util/InvalidParameterException.h>

#include <nidas/linux/usbtwod/usbtwod.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Base class for PMS 2D particle probes on a USB interface.  Covers
 * both the Fast2DC and the white converter box for older 2D probes.
 */
class TwoD_USB : public DSMSensor, public DerivedDataClient
{

public:
    TwoD_USB(std::string name);
    virtual ~TwoD_USB();

    IODevice *buildIODevice();

    SampleScanner *buildSampleScanner();

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags);

    void close();

    int getTASRate() const { return _tasRate; }

    void setTASRate(int val) { _tasRate = val; }

    /**
     * Called by post-processing code
     *
     * @throws  nidas::util::InvalidParameterException
     */
    void init();

    virtual void
    derivedDataNotify(const nidas::core:: DerivedDataReader * s);

    void printStatus(std::ostream& ostr);

    /**
     * Build the struct above from the true airspeed (in m/s).  Encodes
     * for Analog Devices AD5255.
     * @param t2d the Tap2D to be filled
     * @param tas the true airspeed in m/s
     */
   // virtual int TASToTap2D(Tap2D * t2d, float tas);
    virtual int TASToTap2D(void * t2d, float tas);

    /**
     * Reverse the true airspeed encoding.  Used to extract TAS from
     * recorded records.  The *v1 method is for the first generation
     * TAS clock, the second generation gives finer resolution (07/01/2009).
     * Second generation chip is an Analog Devices AD5255.
     * @param t2d the Tap2D to extract from.
     * @param the probe frequency.
     * @returns true airspeed in m/s.
     * @todo 2DP is going to need an extra divide by 10 at the end.
     */
    virtual float
    Tap2DToTAS(const Tap2D * t2d) const;
    /**
     * The first generation used the Maxim 5418 chip.  This is for legacy
     * data (PACDEX through VOCALS).
     */
    virtual float
    Tap2DToTAS(const Tap2Dv1 * t2d) const;


    virtual int NumberOfDiodes() const = 0;


protected:

    // Probe produces Big Endian.
    static const nidas::util::EndianConverter * bigEndian;

    // Tap2D value sent back from driver has little endian ntap value
    static const nidas::util::EndianConverter * littleEndian;

    /**
     * Initialize parameters for real-time and post-processing.
     */
    virtual void init_parameters();

    /**
     * Encode and send the true airspeed to the USB driver, which will
     * in turn send it to the probe.
     */
    virtual void sendTrueAirspeed(float tas);

    /// Probe name (e.g. Fast2DC, HVPS, TwoDS);
    std::string _name;

    /// Common processing utilities for OAP probes.
    TwoD_Processing *_processor;

    /**
     * How often to send the true air speed.
     * Probes also send back the shadowOR when they receive
     * the true airspeed, so in general this is also the
     * receive rate of the shadowOR.
     */
    int _tasRate;

    unsigned int _tasOutOfRange;

//@{
    /**
     * Shadow OR sample ID.  Shadow-OR is the total number of particle triggers
     * the probes saw, regardless of how many images were downloaded.  The 32 bit
     * 2D probes which use the white USB box, don't send shadow-OR via USB.  So
     * this sample ID is only used by the Fast2DC.  We need to stash sample ID's
     * since the XML will not quite look the same between Fast2DC and 2Ds using
     * the white USB box.  The sample IDs will be acquired in the formDOM looking
     * for specific (read hard-coded) string names.  Not sure what the best approach
     * is....
     */
    dsm_sample_id_t _sorID;
//@}

    /**
     * True air speed, received from IWGADTS feed.
     */
    float _trueAirSpeed;

    static const float DefaultTrueAirspeed;

private:

    /** No copying. */
    TwoD_USB(const TwoD_USB&);

    /** No copying. */
    TwoD_USB& operator=(const TwoD_USB&);
};

}}}             // namespace nidas namespace dynld namespace raf

#endif
