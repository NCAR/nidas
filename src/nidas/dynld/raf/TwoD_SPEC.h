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

#ifndef _nidas_dynld_raf_twod_spec_h_
#define _nidas_dynld_raf_twod_spec_h_

#include <nidas/dynld/raf/TwoD_Processing.h>
#include <nidas/dynld/UDPSocketSensor.h>

#include <nidas/util/InvalidParameterException.h>


using namespace nidas::core;


class SpecDecompress;

namespace nidas { namespace dynld { namespace raf {


/**
 * Base class for SPEC optical array probe on a UDP interface.
 */
class TwoD_SPEC : public UDPSocketSensor
{

public:
    TwoD_SPEC(std::string name);
    ~TwoD_SPEC();

    /**
     * Process a single 2D record generating histogram of counts data.  Two
     * histograms of data are generated, using different algorithms: a) the 1D
     * array emulates a 260X, height only and any particle touching the edge
     * is rejected. b) 2D array uses max(widht, height) of particle for
     * particles which do not touch the edge and the center-in method for
     * reconstructing particles which do touch an edge diode.
     *
     * @param samp is the sample data.
     * @param results is the output result array.
     * @see _counts_1D
     * @see _counts_2D
     * @returns whether samples were output.
     */
    bool process(const Sample * samp, std::list < const Sample * >&results);
    bool processHousekeeping(const Sample * samp, std::list < const Sample * >&results);
    bool processImageRecord(const Sample * samp, std::list < const Sample * >&results);

    /**
     * Number of diodes in the probe array.  This is also the bits-per-slice
     * value.  Traditional PMS 2D probes have 32 diodes, the SPEC probes have
     * 128, and the NCAR Fast2DC has 64.
     * @returns the number of bits per data slice.
     */
    virtual int NumberOfDiodes() const { return 128; }

    /**
     * The probe resolution in micrometers.  Probe resolution is also the diameter
     * of the each diode.  Typical values are 25 for the 2DC and 200
     * micrometers for the 2DP.
     * @returns The probe resolution in micrometers.
     */
    unsigned int getResolutionMicron() const { return _processor->getResolutionMicron(); }


    /**
     * Called by post-processing code
     *
     * @throws  nidas::util::InvalidParameterException
     */
    void init();

    void printStatus(std::ostream& ostr);


protected:

    /// Probe nick name.
    std::string _name;


    TwoD_Processing *_processor;

    SpecDecompress *_spec;

    /**
     * buffer space for data.  We'll allocate it once.
     */
    uint16_t *_compressedParticle;
    uint8_t *_uncompressedParticle;

    uint16_t    _prevParticleID;

    unsigned long long _timingWordMask;

    /// This used for arithmetic.  Type32 is 2 words, Type48 is 3 words.
    int _timingWordSize;

//@{
    /**
     * Sync and overload words/masks.
     */
    static const unsigned char _syncString[];
    static const unsigned char _blankString[];
//@}



private:

    /** No copying. */
    TwoD_SPEC(const TwoD_SPEC&);

    /** No copying. */
    TwoD_SPEC& operator=(const TwoD_SPEC&);
};


}}}                     // namespace nidas namespace dynld namespace raf

#endif
