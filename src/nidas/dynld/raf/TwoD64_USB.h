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

#ifndef _nidas_dynld_raf_2d64_usb_h_
#define _nidas_dynld_raf_2d64_usb_h_

#include <nidas/dynld/raf/TwoD_USB.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Class for the USB Fast-2DC.  This probe has a USB 2.0 interface
 * built in.  This probe has 64 diodes instead of the standard 32.
 */
class TwoD64_USB : public TwoD_USB
{
public:
    TwoD64_USB();
    ~TwoD64_USB();

    bool process(const Sample * samp, std::list < const Sample * >&results)
        throw();

    /**
     * Return bits-per-slice; same as the number of diodes in the probe.
     */
    virtual int NumberOfDiodes() const { return 64; }
  
protected:

    void init_parameters()
        throw(nidas::util::InvalidParameterException);

    /**
     * Process the Shadow-OR sample from the probe.
     */
    bool processSOR(const Sample * samp, std::list < const Sample * >&results)
        throw();

    void scanForMissalignedSyncWords(const Sample * samp, const unsigned char * sp) const;

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
    bool processImageRecord(const Sample * samp,
	std::list < const Sample * >&results, int stype) throw();

//@{
    /**
     * Sync and overload words/masks.
     */
#ifdef THE_KNIGHTS_WHO_SAY_NI
    static const unsigned long long _syncMask, _syncWord, _overldWord;
#endif
    static const unsigned char _syncString[];
    static const unsigned char _overldString[];
    static const unsigned char _blankString[];
//@}

private:
    /**
     * Set to true if last slice was a blank line.
     */
    bool _blankLine;

    // Save previous time word with ability to save across records.
    long long prevTimeWord;
};

}}}       // namespace nidas namespace dynld namespace raf
#endif
