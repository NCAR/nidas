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
 * Two-d particle probe on a USB interface.
 */
class TwoD64_USB:public TwoD_USB
{

public:
    TwoD64_USB();
    ~TwoD64_USB();   

    void fromDOMElement(const xercesc::DOMElement *)
        throw(nidas::util::InvalidParameterException);

    bool
    process(const Sample * samp,
                std::list < const Sample * >&results)
     throw();

    /**
     *
     * It is 64 bit images
     *
     **/
    virtual int getBitn() {return 64;}

  
private:

    bool processSOR(const Sample * samp,
                     std::list < const Sample * >&results)
     throw();
    bool processImage(const Sample * samp,
                     std::list < const Sample * >&results)
     throw();

    static const long long _syncMask, _syncWord;
    /**
     * Pixel words from probe are big-endian long longs.
     * Convert the expected syncWord to big-endian for comparison.
     */
    long long _syncMaskBE, _syncWordBE;

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
