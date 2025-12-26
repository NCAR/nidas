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

#ifndef _nidas_dynld_raf_2d_processing_h_
#define _nidas_dynld_raf_2d_processing_h_

#include <nidas/core/Sample.h>
#include <nidas/core/DSMSensor.h>


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Base class for SPEC optical array probe processing.
 */
class TwoD_Processing
{

public:
    TwoD_Processing(std::string name, int nDiodes, DSMSensor *sensor);
    virtual ~TwoD_Processing();

    void init();
    void init_parameters();

    /**
     * Number of diodes in the probe array.  This is also the bits-per-slice
     * value.  Traditional 2D probes have 32 diodes, the SPEC probes have 128,
     * and the Fast2DC has 64.
     * @returns the number of bits per data slice.
     */
    virtual int NumberOfDiodes() const { return _nDiodes; }

    /**
     * The probe resolution in meters.  Probe resolution is also the diameter
     * of the each diode.
     * @returns The probe resolution in meters.
     */
    float getResolution() const { return _resolutionMeters; }

    /**
     * The probe resolution in micrometers.  Probe resolution is also the diameter
     * of the each diode.  Typical values are 25 for the 2DC and 200
     * micrometers for the 2DP.  Ten um for SPEC 2DS and 150 for SPEC HVPS.
     * @returns The probe resolution in micrometers.
     */
    unsigned int getResolutionMicron() const { return _resolutionMicron; }


//protected:
    class Particle
    {
    public:
        Particle() : height(0), width(0), area(0), edgeTouch(0), liveTime(0), dofReject(false) { } ;
        void zero() { height = width = area = liveTime = 0; edgeTouch = 0; dofReject = false; }

        /// Max particle height, along diode array.
        unsigned int height;
        /// Max particle length, along flight path.
        unsigned int width;
	/**
         * Actual number of shadowed diodes.  This can be misleading if there
         * are holes in the particle, poisson spot, etc.
         */
        unsigned int area;
        /**
         * Was an edge diode triggered.  0=no, 0x0f=bottom edge, 0xf0=top edge,
         * 0xff=both edge diodes.
         */
        unsigned char edgeTouch;
        /**
	 * Amount of time consumed by the particle as it passed through the
	 * array.  Basically width * tas-clock-pulses.
	 */
        unsigned int liveTime;

        /// Depth Of Field Reject? Last bit of sync word.
        bool dofReject;
    } ;

    /// Probe name
    std::string _name;

    /// Parent sensor
    DSMSensor *_sensor;

    /**
     * Process a slice and update the Particle struct area, edgeTouch, width
     * and height.
     * @param p is particle info class.
     * @param slice is a pointer to the start of the slice, in big-endian and
     * uncomplemented.
     */
    virtual void processParticleSlice(Particle& p, const unsigned char * slice);

    /**
     * Look at particle stats/info and decide whether to accept or reject.
     * @param p is the particle information.
     * @param resolutionUsec is the current probe clocking rate.
     */
    virtual void countParticle(const Particle& p, float resolutionUsec);

//@{
    /**
     * Accept/reject criteria are in these functions.
     * @param p is particle info class.
     * @returns boolean whether the particle should be rejected.
     */
    virtual bool acceptThisParticle1D(const Particle& p) const;
    virtual bool acceptThisParticle2D(const Particle& p) const;
//@}

//@{
    /**
     * Send derived data and reset.  The process() method for image data is
     * to build size-distribution histograms for 1 second of data and then
     * send that.
     * @param results is the output results.
     * @param nextTimeTag is the timetag of a sample that is past
     *  the end of the current histogram period.  The end
     */
    virtual void createSamples(dsm_time_t nextTimeTag,std::list<const Sample *>&results) throw();

    /**
     * Clear size_dist arrays.
     */
    virtual void clearData();
//@}

    /**
     * Number of image blocks processed by driver at time of last printStatus.
     */
    unsigned int _numImages;

    /**
     * Time of last printStatus.
     */
    long long _lastStatusTime;

    unsigned int _nDiodes;

//@{
    /**
     * Probe resolution in meters.  Acquired from XML config file.
     */
    float _resolutionMeters;

    /**
     * Probe resolution in micrometers.  Acquired from XML config file.
     */
    unsigned int _resolutionMicron;
//@}

    dsm_sample_id_t _1dcID, _2dcID;

//@{
    /**
     * Arrays for size-distribution histograms.
     */
    unsigned int * _size_dist_1D;
    unsigned int * _size_dist_2D;

    /**
     * Amount of time probe was inactive or amount of time consumed by rejected
     * particles.  nimbus will then subtract this deadtime out of the sample
     * volume.
     */
    float _dead_time;
//@}

//@{
    /**
     * Statistics variables for processRecord().
     */
    unsigned int _totalRecords;
    unsigned int _totalParticles;
    unsigned int _rejected1D_Cntr, _rejected2D_Cntr;
    unsigned int _overLoadSliceCount;
    unsigned int _overSizeCount_2D;
    unsigned int _misAligned;
    unsigned int _suspectSlices;
    unsigned int _recordsPerSecond;
//@}

    /// total # of pixels shadoweded in the time period.   per second...
    unsigned int _totalPixelsShadowed;

    /** Time from previous record.  Time belongs to end of record it came with,
     * or start of the next record.  Save it so we can use it as a start.
     */
    dsm_time_t _prevTime;

    /** The end time of the current histogram. */
    long long _histoEndTime;

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
    Particle _particle;

    /**
     * If XML file has a variable RPS (records per second) defined, then _nextravalues
     * will be 2, otherwise 1.  This keeps us from having to retrofit old XML files.
     * @see init()
     * @see process()
     */
    int _nextraValues;

    /**
     * In case of mis-aligned data, we may need to save some bytes
     * at the end of an image block to pre-pend to the next block.
     * @param cp: Pointer to next byte in the image block to be saved.
     * @param eod: Pointer to one-past-the-end of the image block.
     * If cp is equal to eod, then nothing is saved, and this
     * function does not need to be called.
     */
    void saveBuffer(const unsigned char * cp, const unsigned char * eod);

    /**
     * Derived Classes should call this at the beginning of
     * processing an image block.  If bytes were saved from the
     * end of the last image block, then the pointers are
     * adjusted to point to a new buffer that contains the saved and
     * the new data.
     * @param cp: Pointer to pointer of first slice in the new image block to be processed,
     *      after the TAS word.
     * @param eod: Pointer to pointer to one-past-the-end of the image block.
     */
    void setupBuffer(const unsigned char ** cp, const unsigned char ** eod);

    /**
     * The saved buffer.
     */
    unsigned char * _saveBuffer;

    /**
     * How many bytes were last saved.
     */
    int _savedBytes;

    /**
     * Size of the saved buffer.
     */
    int _savedAlloc;


private:

    /** No copying. */
    TwoD_Processing(const TwoD_Processing&);

    /** No copying. */
    TwoD_Processing& operator=(const TwoD_Processing&);
};


template <typename T, typename V>
inline
void
stream_histogram(T& out, V* sizedist, unsigned int nbins)
{
    bool zeros = true;
    for (unsigned int i = 0; i < nbins; ++i)
    {
        if (sizedist[i] == 0)
        {
            out << ".";
            zeros = true;
        }
        else if (zeros)
        {
            out << sizedist[i];
            zeros = false;
        }
        else
        {
            out << ",";
            out << sizedist[i];
        }
    }
}

}}}                     // namespace nidas namespace dynld namespace raf

#endif
