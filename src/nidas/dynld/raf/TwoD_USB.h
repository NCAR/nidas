/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD_USB.h $

 ******************************************************************
*/

#ifndef _nidas_dynld_raf_2d_usb_h_
#define _nidas_dynld_raf_2d_usb_h_

#include <nidas/core/DSMSensor.h>
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
    TwoD_USB();
    ~TwoD_USB();

    IODevice *buildIODevice() throw(nidas::util::IOException);

    SampleScanner *buildSampleScanner();

    int getDefaultMode() const
    {
        return O_RDWR;
    }

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags) throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    int getTASRate() const { return _tasRate; }

    void setTASRate(int val) { _tasRate = val; }
    
    /**
     * The probe resolution in meters.  Probe resolution is also the diameter
     * of the each diode.  Typical values are 25 for the 2DC and 200
     * micrometers for the 2DP.
     * @returns The probe resolution in meters.
     */
    float getResolution() const { return _resolution; }
     
    /**
     * This the same as number of diodes in the probe.
     * @returns the number of bits per data slice.
     */
    virtual int numberBitsPerSlice() const = 0;

    void fromDOMElement(const xercesc::DOMElement *)
        throw(nidas::util::InvalidParameterException);

    bool
    process(const Sample * samp, std::list < const Sample * >&results)
        throw();

    virtual void
    derivedDataNotify(const nidas::core:: DerivedDataReader * s)
        throw();

    void printStatus(std::ostream& ostr) throw();

    /**
     * Build the struct above from the true airspeed (in m/s)
     * @param t2d the Tap2D to be filled
     * @param tas the true airspeed in m/s
     */
    int TASToTap2D(Tap2D * t2d, float tas);

    /**
     * Reverse the true airspeed encoding.  Used to extract TAS from
     * recorded records.
     * @param t2d the Tap2D to extract from.
     * @param the probe frequency.
     * @returns true airspeed in m/s.
     */
    virtual float
    Tap2DToTAS(const Tap2D * t2d, float frequency) const
    { return (1.0e6 / (1.0 - ((float)t2d->ntap / 255))) * frequency; }


protected:

    // Probe produces Big Endian.
    static const nidas::util::EndianConverter * bigEndian;

    /**
     * Encode and send the true airspeed to the USB driver, which will
     * in turn send it to the probe.
     */
    void sendTrueAirspeed(float tas) throw(nidas::util::IOException);

    /**
     * Probe resolution in meters.  Acquired from XML config file.
     */
    float _resolution;

    void addSampleTag(SampleTag * tag)
     throw(nidas::util::InvalidParameterException);

    
    /**
     * How often to send the true air speed. 
     * Probes also send back the shadowOR when they receive
     * the true airspeed, so in general this is also the
     * receive rate of the shadowOR.
     */
    int _tasRate;

    /**
     * Number of image blocks processed by driver at time of last printStatus.
     */
    size_t _numImages;

    /**
     * Time of last printStatus.
     */
    long long _lastStatusTime;

    /**
     * Number of diodes in this probe, same as number of bits per slice.
     */
    size_t nDiodes;

    /**
     * Clear size_dist arrays.
     */
    void clearData();

    /**
     * Arrays for size-distribution histograms.
     */
    float * size_dist_1DC;
    float * size_dist_2DC;

    /**
     * Amount of time probe was inactive or amount of time consumed by rejected
     * particles.  nimbus will then subtract this deadtime out of the sample
     * volume.
     */
    float dead_time_1DC;
    float dead_time_2DC;

    /* Time from previous record.  Time belongs to end of record it came with,
     * or start of the next record.  Save it so we can use it as a start.
     * Units are milliseconds.
     */
    unsigned long long prevTime;

    /* The second for which we are accumulating the histograms.  Assuming we
     * are producing 1 sample per second histograms.
     */
    unsigned long long nowTime;

    /**
     * Send derived data and reset.  The process() method for image data is
     * to build size-distribution histograms for 1 second of data and then
     * send that.
     * @param timeTag is the timeTag for this sample.
     * @param results is the output results.
     */
    void sendData(dsm_time_t timeTag,
                        std::list < const Sample * >&results) throw();

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
