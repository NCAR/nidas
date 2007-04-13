/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/UHSAS_Serial.h $

*/

#ifndef NIDAS_DYNLD_RAF_UHSAS_SERIAL_H
#define NIDAS_DYNLD_RAF_UHSAS_SERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading the UHSAS probe.
 * RS-232 @ 115,200 baud.
 */
class UHSAS_Serial : public DSMSerialSensor
{
public:

  UHSAS_Serial() : DSMSerialSensor() {}

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();


protected:


};

}}}	// namespace nidas namespace dynld raf

#endif
