/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoDC_USB.h $

 ******************************************************************
*/

#ifndef _nidas_dynld_raf_2dc_usb_h_
#define _nidas_dynld_raf_2dc_usb_h_

#include <nidas/core/DSMSensor.h>
#include <nidas/core/DerivedDataClient.h>


#include <nidas/util/EndianConverter.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;
/**
 * Two-d particle probe on a USB interface.
 */
class TwoDC_USB:public DSMSensor, public DerivedDataClient
{

public:
    TwoDC_USB();
    ~TwoDC_USB();

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

    void fromDOMElement(const xercesc::DOMElement *)
     throw(nidas::util::InvalidParameterException);

     bool
        process(const Sample * samp,
                std::list < const Sample * >&results)
     throw();

    virtual void
        derivedDataNotify(const nidas::core::
                          DerivedDataReader * s) throw();

private:

     bool processSOR(const Sample * samp,
                     std::list < const Sample * >&results)
     throw();
    bool processImage(const Sample * samp,
                      std::list < const Sample * >&results)
     throw();

    // Probe produces Big Endian.
    static const nidas::util::EndianConverter * fromBig;

    /**
     * Encode and send the true airspeed to the USB driver, which will
     * in turn send it to the probe.
     */
    void sendTrueAirspeed(float tas);

    /**
     * Probe resolution in meters.  Acquired from XML config file.
     */
    double _resolution;

    void addSampleTag(SampleTag * tag)
     throw(nidas::util::InvalidParameterException);

    /**
     * Synchword mask.  This slice/word is written at the end of each particle.
     * 28 bits of synchronization and 36 bits of timing information.
     */
    static const long long _syncMask, _syncWord;

    /**
     * How often to request the shadow OR, in HZ.
     */
    int _sorRate;
};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
