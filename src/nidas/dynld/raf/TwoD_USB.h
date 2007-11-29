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
 * Two-d particle probe on a USB interface.
 */
class TwoD_USB:public DSMSensor, public DerivedDataClient
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

    void fromDOMElement(const xercesc::DOMElement *)
     throw(nidas::util::InvalidParameterException);

     bool
        process(const Sample * samp,
                std::list < const Sample * >&results)
     throw();

    virtual void
        derivedDataNotify(const nidas::core::
                          DerivedDataReader * s) throw();

    void printStatus(std::ostream& ostr) throw();

    /*
     * Build the struct above from the true airspeed (in m/s)
     * @param t2d the Tap2D to be filled
     * @param tas the true airspeed in m/s
     * @param resolution the resolution or diode size, in meters.
     */
    int TASToTap2D(Tap2D * t2d, float tas, float resolution);


protected:

    // Probe produces Big Endian.
    static const nidas::util::EndianConverter * bigEndian;

    /**
     * Encode and send the true airspeed to the USB driver, which will
     * in turn send it to the probe.
     */
    void sendTrueAirspeed(float tas) throw(nidas::util::IOException);

    /**
     * Probe resolution in meters.  Acquired from XML config file.
     */
    double _resolution;

    void addSampleTag(SampleTag * tag)
     throw(nidas::util::InvalidParameterException);

    
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
};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
