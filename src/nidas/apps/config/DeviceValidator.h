#ifndef DEVICE_VALIDATOR_H
#define DEVICE_VALIDATOR_H

#include <string>
#include <map>


class DeviceValidator {

public:

  static DeviceValidator * getInstance() { if (!_instance) _instance = new DeviceValidator(); return _instance; }

  std::string & getDevicePrefix(std::string & key) { return _devMap[key].devicePrefix;}
  unsigned int getMin(std::string & key) { return _devMap[key].min;}
  unsigned int getMax(std::string & key) { return _devMap[key].max;}
  const char *getInterfaceLabel(std::string & key) { return _DeviceDefinition::_InterfaceLabels[_devMap[key].sensorType]; }

protected:
  class _DeviceDefinition {
    friend class DeviceValidator;

    public:
     std::string sensorName;
     std::string devicePrefix;
     unsigned int min;
     unsigned int max;

     enum { SERIAL, ANALOG, UDP, } sensorType;

    private:
     static const char *_InterfaceLabels[];
  };

  std::map<std::string, _DeviceDefinition> _devMap;

  static _DeviceDefinition _Definitions[];

private:
  DeviceValidator();
  static DeviceValidator * _instance;
};



#endif
