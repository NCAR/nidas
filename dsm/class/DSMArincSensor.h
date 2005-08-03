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
#include <set>

// Significant bits masks
//
// 32|31 30|29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11|10  9| 8  7  6  5  4  3  2  1
// --+-----+--------------------------------------------------------+-----+-----------------------
// P | SSM |                                                        | SDI |      8-bit label      
#define m08 0x0003fc00
#define m09 0x0007fc00
#define m10 0x000ffc00
#define m11 0x001ffc00
#define m12 0x003ffc00
#define m13 0x007ffc00
#define m14 0x00fffc00
#define m15 0x01fffc00
#define m16 0x03fffc00
#define m17 0x07fffc00
#define m18 0x0ffffc00
#define m19 0x1ffffc00

// bitmask for the Sign Status Matrix
#define SSM 0x60000000
#define NCD 0x20000000
#define TST 0x40000000

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

// Feet to Meters.
const float FT_MTR  = 0.3048;

// G to m/s2 (ACINS).
const float G_MPS2   = 9.7959;

// Knots to m/s
const float KTS_MS = 0.514791;

// Ft/min to m/s (VSPD)
const float FPM_MPS  = 0.00508;

// Radians to Degrees.
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
    void open(int flags) throw(atdUtil::IOException);

    /** This closes the associated RT-Linux FIFOs. */
    void close() throw(atdUtil::IOException);

    /** Process a raw sample, which in this case means create
     * a list of samples with each sample containing a timetag. */
    bool process(const Sample*, std::list<const Sample*>& result)
      throw();

    /** Display some status information gathered by the driver. */
    void printStatus(std::ostream& ostr) throw();

    /** This contains a switch case for processing all labels. */
    virtual float processLabel(const unsigned long data) = 0;

    /** Extract the ARINC configuration elements from the XML header. */
    void fromDOMElement(const xercesc::DOMElement*)
      throw(atdUtil::InvalidParameterException);

  protected:
    const float _nanf;

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
