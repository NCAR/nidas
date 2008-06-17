/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Sept 2007) $

    $LastChangedRevision: 3650 $

    $First version: dongl $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD32_USB.h $

 ******************************************************************
*/

#ifndef _nidas_dynld_raf_2d32_usb_h_
#define _nidas_dynld_raf_2d32_usb_h_

#include <nidas/dynld/raf/TwoD_USB.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensor class for the standard PMS2D probes where the data signals
 * are converted to USB by a converter box.  These probes have the
 * standard 32 diodes.
 */
class TwoD32_USB : public TwoD_USB 
{
public:
    TwoD32_USB();
    ~TwoD32_USB();

    bool process(const Sample * samp, std::list < const Sample * >&results)
        throw();

    virtual size_t NumberOfDiodes() const { return 32; }


protected:
    bool processImage(const Sample * samp, std::list < const Sample * >&results)
        throw();

//@{
    /*
     * Synchword mask.  This slice/word is written at the end of each particle.
     * ?? bits of synchronization and ?? bits of timing information.
     */
    static const uint32_t _syncMask, _syncWord;
    static const unsigned char _syncChar;
//@}

};  //endof_class

}}}                     // namespace nidas namespace dynld namespace raf
#endif
