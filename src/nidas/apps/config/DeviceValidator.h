#ifndef DEVICE_VALIDATOR_H
#define DEVICE_VALIDATOR_H

#include <string>
#include <map>


class DeviceValidator {

public:
  DeviceValidator();

  class _DeviceDefinition {
    public:
     std::string sensorName;
     std::string devicePrefix;
     unsigned int min;
     unsigned int max;

     operator std::pair<std::string, _DeviceDefinition>() const {
       return std::make_pair(sensorName, *this);
     }
  };

protected:
  std::map<std::string, _DeviceDefinition> _devMap;

};



#endif
