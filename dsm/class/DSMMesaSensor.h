/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

 ******************************************************************
*/
#ifndef DSMMESASENSOR_H
#define DSMMESASENSOR_H

#include <RTL_DSMSensor.h>
#include <atdUtil/InvalidParameterException.h>

namespace dsm {
/**
 * A sensor connected to a MESA port.
 */
class DSMMesaSensor : public RTL_DSMSensor {

public:

  DSMMesaSensor();

  ~DSMMesaSensor();

  /* This opens the associated RT-Linux FIFOs.  */
  void open(int flags) throw(atdUtil::IOException);

  /* This closes the associated RT-Linux FIFOs. */
  void close() throw(atdUtil::IOException);

  /* Extract the MESA configuration elements from the XML header. */
  void fromDOMElement(const xercesc::DOMElement*)
       throw(atdUtil::InvalidParameterException);

  private:

  struct radar_set set_radar;
  struct counters_set set_counter;
  int counter_channels;
  int radar_channels;

};
}
#endif
