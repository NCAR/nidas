/*
 * WICORSensor.h
 *
 *  Created on: Jun 29, 2011
 *      Author: granger
 */

#ifndef WICORSENSOR_H_
#define WICORSENSOR_H_

#include "nidas/dynld/raf/UDPSocketSensor.h"

#include <vector>
#include <string>

namespace nidas {

namespace dynld {

namespace iss {

class WICORSensor : public virtual nidas::dynld::raf::UDPSocketSensor
{
public:
  WICORSensor();

  virtual
  ~WICORSensor();

  virtual void
  addSampleTag(nidas::core::SampleTag* stag)
    throw (nidas::util::InvalidParameterException);

  virtual bool
  process(const nidas::core::Sample*,
        std::list<const nidas::core::Sample*>& result) throw ();

private:
  std::vector<std::string> _patterns;

};

}

}

}

#endif /* WICORSENSOR_H_ */
