#ifndef DEVICE_VALIDATOR_H
#define DEVICE_VALIDATOR_H

#include <string>
#include <map>


class DeviceValidator {

public:

  DeviceValidator * getInstance() { if (!_instance) _instance = new DeviceValidator(); return _instance; }
  class _DeviceDefinition {
    public:
     std::string sensorName;
     std::string devicePrefix;
     unsigned int min;
     unsigned int max;
  };

protected:
  std::map<std::string, _DeviceDefinition> _devMap;

private:
  DeviceValidator();
  static DeviceValidator * _instance;
};



#endif
