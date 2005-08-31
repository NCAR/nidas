/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef DSMARINCSENSOR_H
#define DSMARINCSENSOR_H

#include <arinc.h>
#include <RTL_DSMSensor.h>
#include <atdUtil/InvalidParameterException.h>

// Significant bits masks
//
// 32|31 30|29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11|10  9| 8  7  6  5  4  3  2  1
// --+-----+--------------------------------------------------------+-----+-----------------------
// P | SSM |                                                        | SDI |      8-bit label      

// bitmask for the Sign Status Matrix
#define SSM 0x60000000
#define NCD 0x20000000
#define TST 0x40000000

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

// inHg to mBar
const float INHG_MBAR  = 33.8639;

// ft to meter.
const float FT_MTR  = 0.3048;

// G to m/s2 (ACINS).
const float G_MPS2   = 9.7959;

// knot to m/s
const float KTS_MS = 0.514791;

// ft/min to m/s (VSPD)
const float FPM_MPS  = 0.00508;

// radian to degree.
const float RAD_DEG = 180.0 / 3.14159265358979; 

namespace dsm {

  /**
   * This is sorts a list of Sample tags by rate (highest first)
   * then by label.
   */
  class SortByRateThenLabel {
  public:
    bool operator() (const SampleTag* x, const SampleTag* y) const {
      if ( x->getRate() > y->getRate() ) return true;
      if ( x->getRate() < y->getRate() ) return false;
      if ( x->getId()   < y->getId()   ) return true;
      return false;
    }
  };

  /**
   * A sensor connected to an ARINC port.
   */
  class DSMArincSensor : public RTL_DSMSensor {

  public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    DSMArincSensor();
    ~DSMArincSensor();

    /** This opens the associated RT-Linux FIFOs. */
    void open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    /** This closes the associated RT-Linux FIFOs. */
    void close() throw(atdUtil::IOException);

    /**
     * Perform any initialization necessary for process method.
     */
    void init() throw(atdUtil::InvalidParameterException);

    /** Process a raw sample, which in this case means create
     * a list of samples with each sample containing a timetag. */
    bool process(const Sample*, std::list<const Sample*>& result)
      throw();

    /** Display some status information gathered by the driver. */
    void printStatus(std::ostream& ostr) throw();

    /** This contains a switch case for processing all labels. */
    virtual float processLabel(const long data) = 0;

    /** Extract the ARINC configuration elements from the XML header. */
    /// example XML:
    ///  <arincSensor ID="GPS-GV" class="GPS_HW_HG2021GB02" speed="low" parity="odd">
    void fromDOMElement(const xercesc::DOMElement*)
      throw(atdUtil::InvalidParameterException);

  protected:
    const float _nanf;

    /// A list of which samples are processed.
    int _processed[256];

  private:

    /** channel configuration */
    unsigned int _speed;
    unsigned int _parity;

    /** Transmit blank ARINC labels (used for simulation purposes) */
    bool sim_xmit;
  };

  typedef SampleT<unsigned long> ArincSample;

}

#endif
