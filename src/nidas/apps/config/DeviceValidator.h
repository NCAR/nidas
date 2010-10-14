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
  const char *getInterfaceLabel(std::string & key) { return
  _DeviceDefinition::_InterfaceLabels[ _devMap[key].sensorType ];
  }

protected:
  class _DeviceDefinitionStruct {
    friend class DeviceValidator;

    public:
     std::string sensorName;
     std::string devicePrefix;
     unsigned int min;
     unsigned int max;

     enum { SERIAL, ANALOG, UDP, _MAX } sensorType;

    private:
     static const char *_InterfaceLabels[];
  };

  class _DeviceDefinition : public _DeviceDefinitionStruct {
    public:
     _DeviceDefinition() { min=0; max=0; sensorType=SERIAL; }
     _DeviceDefinition(_DeviceDefinitionStruct & proxy) { sensorName=proxy.sensorName; devicePrefix=proxy.devicePrefix; min=proxy.min; max=proxy.max; sensorType=proxy.sensorType; }
  };

  std::map<std::string, _DeviceDefinition> _devMap;

  static _DeviceDefinitionStruct _Definitions[];

private:
  DeviceValidator();
  static DeviceValidator * _instance;
};



#endif
