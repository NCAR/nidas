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
    float getResolution() const { return _resolutionMeters; }
     
    /**
     * The probe resolution in micrometers.  Probe resolution is also the diameter
     * of the each diode.  Typical values are 25 for the 2DC and 200
     * micrometers for the 2DP.
     * @returns The probe resolution in micrometers.
     */
    size_t getResolutionMicron() const { return _resolutionMicron; }
     
    /**
     * Number of diodes in the probe array.  This is also the bits-per-slice
     * value.  Traditional 2D probes have 32 diodes, the HVPS has 128 and
     * the Fast2DC has 64.
     * @returns the number of bits per data slice.
     */
    virtual size_t NumberOfDiodes() const = 0;

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
     * @todo 2DP is going to need an extra divide by 10 at the end.
     */
    virtual float
    Tap2DToTAS(const Tap2D * t2d) const;


protected:
    class Particle
    {
    public:
        Particle() : height(0), width(0), area(0), edgeTouch(0) { } ;

        /// Max particle height, along diode array.
        size_t height;
        /// Max particle length, along flight path.
        size_t width;
	/**
         * Actual number of shadowed diodes.  This can be misleading if there
         * are holes in the particle, poisson spot, etc.
         */
        size_t area;
        /**
         * Was an edge diode triggered.  0=no, 0x0f=bottom edge, 0xf0=top edge,
         * 0xff=both edge diodes.
         */
        unsigned char edgeTouch;
        /**
	 * Amount of time consumed by the particle as it passed through the
	 * array.  Basically width * tas-clock-pulses.
	 */
        unsigned long liveTime;
    } ;

    // Probe produces Big Endian.
    static const nidas::util::EndianConverter * bigEndian;

    /**
     * Initialize processing variables.  Was unable to put this in the
     * c-tor due to call of a pure virtual method.
     */
    virtual void init_processing();

    /**
     * Encode and send the true airspeed to the USB driver, which will
     * in turn send it to the probe.
     */
    virtual void sendTrueAirspeed(float tas) throw(nidas::util::IOException);

    void addSampleTag(SampleTag * tag)
     throw(nidas::util::InvalidParameterException);

    /**
     * Process a slice and update the Particle struct area, edgeTouch, width
     * and height.
     * @param p is particle info class.
     * @param slice is a pointer to the start of the slice, in big-endian and
     * uncomplemented.
     */
    virtual void processParticleSlice(Particle * p, const unsigned char * slice);

    /**
     * Look at particle stats/info and decide whether to accept or reject.
     * @param p is the particle information.
     * @param frequency is the current probe clocking rate.
     */
    virtual void countParticle(Particle * p, float frequency);

//@{
    /**
     * Accept/reject criteria are in these functions.
     * @param p is particle info class.
     * @returns boolean whether the particle should be rejected.
     */
    virtual bool acceptThisParticle1D(const Particle * p) const;
    virtual bool acceptThisParticle2D(const Particle * p) const;
//@}
    
//@{
    /**
     * Send derived data and reset.  The process() method for image data is
     * to build size-distribution histograms for 1 second of data and then
     * send that.
     * @param timeTag is the timeTag for this sample.
     * @param results is the output results.
     */
    virtual void sendData(dsm_time_t timeTag,
                        std::list < const Sample * >&results) throw();

    /**
     * Clear size_dist arrays.
     */
    virtual void clearData();
//@}

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

//@{
    /**
     * Probe resolution in meters.  Acquired from XML config file.
     */
    float _resolutionMeters;

    /**
     * Probe resolution in micrometers.  Acquired from XML config file.
     */
    size_t _resolutionMicron;
//@}

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

    dsm_sample_id_t _1dcID, _2dcID;
//@}

//@{
    /**
     * Arrays for size-distribution histograms.
     */
    size_t * _size_dist_1D;
    size_t * _size_dist_2D;

    /**
     * Amount of time probe was inactive or amount of time consumed by rejected
     * particles.  nimbus will then subtract this deadtime out of the sample
     * volume.
     */
    float _dead_time_1D;
    float _dead_time_2D;
//@}

//@{
    /**
     * Statistics variables for processRecord().
     */
    size_t _totalRecords;
    size_t _totalParticles;
    size_t _rejected1D_Cntr, _rejected2D_Cntr;
    size_t _overLoadSliceCount;
    size_t _overSizeCount_2D;
//@}

//@{
    /* Time from previous record.  Time belongs to end of record it came with,
     * or start of the next record.  Save it so we can use it as a start.
     * Units are milliseconds.
     */
    unsigned long long _prevTime;

    /* The second for which we are accumulating the histograms.  Assuming we
     * are producing 1 sample per second histograms.
     */
    unsigned long long _nowTime;
//@}

    /**
     * Area of particle rejection ratio.  Actual area of particle divided
     * area of bounding box must be greater than this.
     * @see acceptThisParticle1D()
     */
    float _twoDAreaRejectRatio;

    /**
     * Current particle information.  This is in here, since a particle can cross
     * samples/records.
     */
    Particle * _cp;
};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
