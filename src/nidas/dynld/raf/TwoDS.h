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

#ifndef _nidas_dynld_raf_2ds_h_
#define _nidas_dynld_raf_2ds_h_

#include <nidas/dynld/raf/TwoD_Processing.h>
#include <nidas/dynld/UDPSocketSensor.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Base class for SPEC 2DS optical array probe on a USB interface.
 * Perhaps can be split out into a base class and sub-classes for
 * 2DS & HVPS.
 */
class TwoDS : public UDPSocketSensor, public TwoD_Processing
{

public:
    TwoDS();
    ~TwoDS();

    bool process(const Sample * samp, std::list < const Sample * >&results);

    /**
     * Number of diodes in the probe array.  This is also the bits-per-slice
     * value.  Traditional 2D probes have 32 diodes, the HVPS has 128 and
     * the Fast2DC has 64.
     * @returns the number of bits per data slice.
     */
    virtual int NumberOfDiodes() const { return 128; }

    /**
     * Called by post-processing code
     *
     * @throws  nidas::util::InvalidParameterException
     */
    void init();

    void printStatus(std::ostream& ostr);


protected:
    /**
     * Initialize parameters for real-time and post-processing.
     */
    virtual void init_parameters();

    /**
     * Process the UDP ASCII packet.
     */
    bool processHousekeeping(const Sample * samp, std::list < const Sample * >&results);

    /**
     * Process a single 2D record generating size-distribution data.  Two
     * size-distribution data are generated: a) the 1D array emulates a 260X,
     * height only and any particle touching the edge is rejected. b) 2D
     * array uses max(widht, height) of particle for particles which do not
     * touch the edge and the center-in method for reconstructing particles
     * which do touch an edge diode.
     *
     * @param samp is the sample data.
     * @param results is the output result array.
     * @see _size_dist_1D
     * @see _size_dist_2D
     * @returns whether samples were output.
     */
    bool processImageRecord(const Sample * samp, std::list < const Sample * >&results);

//@{
    /**
     * Sync and overload words/masks.
     */
    static const unsigned char _syncString[];
    static const unsigned char _blankString[];
//@}



private:

    /** No copying. */
    TwoDS(const TwoDS&);

    /** No copying. */
    TwoDS& operator=(const TwoDS&);
};


}}}                     // namespace nidas namespace dynld namespace raf

#endif
