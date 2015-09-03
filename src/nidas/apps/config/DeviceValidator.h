/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
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
