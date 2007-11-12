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
 * Two-d particle probe on a USB interface.
 */
class TwoD32_USB:public TwoD_USB 
{

public:
    TwoD32_USB();
    ~TwoD32_USB();

    bool
        process(const Sample * samp,
                std::list < const Sample * >&results)
     throw();

    /**
     *
     * It is 32 bit images
     *
     **/
    virtual int getBitn() {return 32;}



private:

    bool processImage(const Sample * samp,
                      std::list < const Sample * >&results)
     throw();

     /*
     * Synchword mask.  This slice/word is written at the end of each particle.
     * ?? bits of synchronization and ?? bits of timing information.
     */
    static const unsigned long _syncMask, _syncWord;
    unsigned long _syncWordBE, _syncMaskBE;

    
};  //endof_class

}}}                     // namespace nidas namespace dynld namespace raf
#endif
