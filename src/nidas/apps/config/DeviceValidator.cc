#include "DeviceValidator.h"
#include <iostream>
#include <fstream>


const char * DeviceValidator::_DeviceDefinitionStruct::_InterfaceLabels[] = { "Channel", "Board", "Port", };

DeviceValidator::_DeviceDefinitionStruct DeviceValidator::_Definitions[] = {

{"", "", 0, 0, DeviceValidator::_DeviceDefinition::SERIAL},
{"Analog", "/dev/ncar_a2d", 0, 9, DeviceValidator::_DeviceDefinition::ANALOG},
{"ADC-GV", "/dev/arinc", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Butanol_CN_Counter", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"CCN", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"CDP", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"CFDC", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"CMIGITS3", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"COMR", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"CORAW", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"D_GPS", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"DewPointer", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
//{"Fast2DC", "/dev/usbtwod", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Fast2DC", "/dev/usbtwod_64_", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"GPS-GV", "/dev/arinc", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Garmin_GPS", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"HoneywellPPT", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"IRIG", "/dev/irig", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"IRS-C130", "/dev/arinc", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"IRS-GV", "/dev/arinc", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"LAMS", "/dev/lams", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Mensor_6100", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"NOAAPANTHER", "inet::", 20000, 65535, DeviceValidator::_DeviceDefinition::UDP},
{"NOAASP2", "inet::", 20000, 65535, DeviceValidator::_DeviceDefinition::UDP},
{"NOAAUCATS", "inet::", 20000, 65535, DeviceValidator::_DeviceDefinition::UDP},
{"NOAA_CSD_O3", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Novatel_GPS", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"OphirIII", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Paro_DigiQuartz_1000", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"QCLS", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"S100", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"S200", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"S300", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"SP2", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"TDLH2O", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"TwoDP", "/dev/usbtwod_32_", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"TwoD_House", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"UHSAS", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"UHSAS_CU", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"VCSEL", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},
{"Water_CN_Counter", "/dev/ttyS", 0, 9, DeviceValidator::_DeviceDefinition::SERIAL},

};

DeviceValidator * DeviceValidator::_instance = NULL;

DeviceValidator::DeviceValidator() //: 
       //_devMap(_Definitions, _Definitions + sizeof(_Definitions) / sizeof(_DeviceDefinition)) 
{

  int n =  sizeof(_Definitions) / sizeof(_DeviceDefinition);
  for (int i = 0; i < n;  i++) {
    //_devMap.insert ( _devMap.rbegin(), std::pair<std::string, _DeviceDefinition>(_Definitions[i].sensorName, _Definitions[i]));
    _devMap.insert ( std::pair<std::string, _DeviceDefinition>(_Definitions[i].sensorName, _Definitions[i]));
  }
  for (std::map<std::string, _DeviceDefinition>::iterator it = _devMap.begin(); it != _devMap.end(); it++) {

      std::cerr << it->first << std::endl;
//    cerr << it->first << " " << (_DeviceDefinition)it->second.devicePrefix << " " << (_DeviceDefinition)it->second.min << " " << (_DeviceDefinition)it->second.max << endl;

  }

}
