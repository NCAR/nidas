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
#define m01 0x00000400
#define m02 0x00000c00
#define m03 0x00001c00
#define m04 0x00003c00
#define m05 0x00007c00
#define m06 0x0000fc00
#define m07 0x0001fc00
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

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

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
