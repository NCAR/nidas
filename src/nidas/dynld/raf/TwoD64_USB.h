/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD64_USB.h $

 ******************************************************************
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

    void fromDOMElement(const xercesc::DOMElement *)
        throw(nidas::util::InvalidParameterException);

    bool process(const Sample * samp, std::list < const Sample * >&results)
        throw();

    /**
     * Return bits-per-slice; same as the number of diodes in the probe.
     */
    virtual size_t NumberOfDiodes() const { return 64; }

  
protected:
    /**
     * Process the Shadow-OR sample from the probe.
     */
    bool processSOR(const Sample * samp, std::list < const Sample * >&results)
        throw();

    void scanForMissalignedSyncWords(unsigned char * sp) const;

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
	std::list < const Sample * >&results) throw();

//@{
    /**
     * Sync and overload words/masks.
     */
#ifdef THE_KNIGHTS_WHO_SAY_NI
    static const unsigned long long _syncMask, _syncWord, _overldWord;
#endif
    static const unsigned char _syncString[3], _overldString[3];
//@}
};

}}}       // namespace nidas namespace dynld namespace raf
#endif
