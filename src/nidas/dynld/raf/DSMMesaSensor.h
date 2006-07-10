/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_DSMMESASENSOR_H
#define NIDAS_DYNLD_RAF_DSMMESASENSOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A sensor connected to a MESA port.
 */
class DSMMesaSensor : public DSMSensor {

public:

  DSMMesaSensor();

  ~DSMMesaSensor();

  IODevice* buildIODevice() throw(nidas::util::IOException);

  SampleScanner* buildSampleScanner();

  /* This opens the associated RT-Linux FIFOs.  */
  void open(int flags) throw(nidas::util::IOException);

  /* This closes the associated RT-Linux FIFOs. */
  void close() throw(nidas::util::IOException);

  /* Extract the MESA configuration elements from the XML header. */
  void fromDOMElement(const xercesc::DOMElement*)
       throw(nidas::util::InvalidParameterException);

  private:

  struct radar_set set_radar;
  struct counters_set set_counter;
  int counter_channels;
  int radar_channels;

  FILE* fdMesaFPGAfile;
  int fdMesaFPGAfifo;
  signed long ImageLen ; /* Number of bytes in device image portion of 
                                 file. */
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
