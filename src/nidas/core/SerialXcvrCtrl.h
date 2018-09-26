// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab: 
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2018, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_CORE_SERIALXCVRCONTROL_H
#define NIDAS_CORE_SERIALXCVRCONTROL_H

#include <ftdi.h>
#include <sstream>

namespace n_u = nidas::util;
namespace nidas { namespace core {

/**
 * types of serial ports
 */
enum PORT_TYPES {LOOPBACK=0, RS232=232, RS422=422, RS485_FULL=422, RS485_HALF=484};

/*
 * This enum specifies the ports in the DSM.
 */
enum PORT_DEFS {PORT0=0, PORT1, PORT2, PORT3, PORT4, PORT5, PORT6, PORT7};
/*
 * Serial termination settings for RS422/RS485
 */
enum TERM {NO_TERM=0, TERM_120_OHM};

/*
 * Sensor power setting
 */
typedef enum {SENSOR_POWER_OFF, SENSOR_POWER_ON} SENSOR_POWER_STATE;

/*  
 *  struct XcvrConfig is used to house and contain all the parameters which are used to 
 *  set up the EXAR SP339 serial line driver/transceiver chip. It is intended to be used extensively by 
 *  the auto-config base classes as an easy means to change the transceiver mode of operation.
 */
struct XcvrConfig {
    XcvrConfig() : port(PORT0), portType(RS232), termination(NO_TERM), sensorPower(SENSOR_POWER_ON) {}
    XcvrConfig(PORT_DEFS initPortID, PORT_TYPES initPortType, TERM initTerm=NO_TERM, SENSOR_POWER_STATE initPower=SENSOR_POWER_ON) 
        : port(initPortID), portType(initPortType), termination(initTerm), sensorPower(initPower) {}
    bool operator!=(const XcvrConfig& rRight) const {return !((*this) == rRight);}
    bool operator==(const XcvrConfig& rRight) const
        {return (this == &rRight) || (port == rRight.port
        		                      && portType == rRight.portType
									  && termination == rRight.termination
									  && sensorPower == rRight.sensorPower);}
    void print();
    std::ostream& operator <<(std::ostream outStrm);

    PORT_DEFS port;
    PORT_TYPES portType;
    TERM termination;
    SENSOR_POWER_STATE sensorPower;
};

std::ostream& operator <<(std::ostream& rOutStrm, const XcvrConfig& rObj);

/*
 ********************************************************************
 ** SerialXcvrCtrl is a class which controls the Exar SP339 serial 
 ** driver chip to configure it for a particular mode of serial I/O
 ** operation. It is primarily used by SerialPortIODevice, but also 
 ** may be used by various utilities.
 **
 ** Operation occurs via a FT4232H USB<->Serial device which has four 
 ** ports. Each port is put into bit bang mode for GPIO operation. Each 
 ** port controls four separate SP339 driver chips, 4 bits per SP339 chip. 
 ** It is intended that up to eight instances of this class 
 ** be instantiated for the current version of the DSM. Future versions 
 ** may require additional instances should multiple Serial Boards be 
 ** installed in the DSM.
 **
 ** Since each instance shares the FT4232H GPIO interface with 3 other 
 ** instances, the individual instances will need to open the device 
 ** until it is successful. This typically won't be a problem because 
 ** sensors which use these serial ports are enumerated one at a time 
 ** in the DSM XML config.
 ********************************************************************
*/
class SerialXcvrCtrl {
public:
    // Constructor needs to know what port is being controlled
    // Constructor uses portID to decide which FTDI interface to 
    // use to control the port type.
    // Bus address is the means by which the USB device is opened.
    // So default them to the values known today, but may be 
    // overridden later.
    SerialXcvrCtrl(const PORT_DEFS portId);
    SerialXcvrCtrl(const PORT_DEFS portId, const PORT_TYPES portType, const TERM termination=NO_TERM, 
                              const SENSOR_POWER_STATE powerState=SENSOR_POWER_ON);
    SerialXcvrCtrl(const XcvrConfig initXcvrConfig);
    // Destructor
    ~SerialXcvrCtrl();

    static bool xcvrCtrlSupported() { return true; } // for now. Need to actually read an eeprom to know
    // This sets the class state to be used by applyXcvrConfig();
    void setXcvrConfig(const PORT_TYPES portType, const TERM term, const SENSOR_POWER_STATE powerState);
    void setXcvrConfig(const XcvrConfig& newXcvrConfig) {_xcvrConfig = newXcvrConfig;}
    // This is the primary client API that does all the heavy lifting  
    // to actually change the SP339 driver port type/mode (RS232, RS422, etc).
    void applyXcvrConfig(const bool readDevice=true);
    // Returns the raw bits already reported by readXcvrConfig() indicating current state of  
    // the port mode, including termination and sensor power
    unsigned char getRawXcvrConfig() {return _rawXcvrConfig;};
    // Returns the raw bits indicating current state of the port mode, including sensor power
    XcvrConfig& getXcvrConfig() {return _xcvrConfig;};
    // Reads the xcvr config from the FTDI chip and put it in _rawXcvrConfig
    void readXcvrConfig();
    // This informs the class as to which USB device to open.
    // This has no effect until the device is closed and then re-opened
    void setBusAddress(const int busId=1, const int deviceId=6);
    // This utility converts a PORT_TYPE to a string
    static const std::string portTypeToStr(const PORT_TYPES portType);
    // This utility converts a string to a PORT_TYPE
    static PORT_TYPES strToPortType(const char* portStr);
    // This utility converts a binary term configuration to a string
    static const std::string termToStr(TERM term)
    {
        switch (term) {
            case NO_TERM:
                return std::string(STR_NO_TERM);
            case TERM_120_OHM:
                return std::string(STR_TERM_120_OHM);
            default:
                std::stringstream sstrm("Unknown termination state: ");
                sstrm << term;
                return sstrm.str();
                break;
        }
    } 
    // This utility converts a binary power configuration to a string
    static const std::string powerStateToStr(SENSOR_POWER_STATE sensorState)
    {
        switch (sensorState) {
            case SENSOR_POWER_OFF: 
                return std::string(STR_POWER_OFF);
                break;
            case SENSOR_POWER_ON: 
                return std::string(STR_POWER_ON);
                break;
            default: 
                std::stringstream sstrm("Unknown sensor power state: ");
                sstrm << sensorState;
                return sstrm.str();
                break;
        }
    } 
    // This utility converts a binary term configuration to a string
    static const std::string rawTermToStr(unsigned char termCfg); 
    // This utility converts a binary power configuration to a string
    static const std::string rawPowerToStr(unsigned char powerCfg); 
    // This utility prints the port types for the assigned port.
    void printXcvrConfig(const bool addNewline=true, const bool readFirst=true);
    // This is a utility to convert an integer to a PORT_DEFS port ID
    // Currently assumes that the portNum is in the range of PORT_DEFS
    static PORT_DEFS int2PortDef(const unsigned portNum)
    {
        return static_cast<PORT_DEFS>(portNum);
    }

protected:
    // shift the port type into the correct location in the GPIO for insertion
    unsigned char adjustBitPosition(const unsigned char bits );
    // assembles the port config bits into a low nibble, ready for shifting.
    unsigned char assembleBits(const PORT_TYPES portType, const TERM term, const SENSOR_POWER_STATE powerState);
    // deduces the FT4232H GPIO interface form the port in _xcvrConfig.
    // need to select the interface based on the specified port 
    // at present, assume 4 bits per port definition
    enum ftdi_interface port2iface();
    // Morphs PORT_TYPES to the SP339 M0/M1 bit definitions
    unsigned char portType2Bits(const PORT_TYPES portType);
    // Morphs the SP339 M0/M1 bit definitions to the associated PORT_TYPE 
    PORT_TYPES bits2PortType(const unsigned char bits);
    // safe FT4232H open - must be bracket all gpio operations!!!
    // returns true if already open or successfully open, false if attempted open fails.
    bool gpioOpen();
    // safe FT4232H close - must bracket all gpio operations!!!
    // returns true if already closed or successfully closed, false if attempted close fails.
    bool gpioClose(bool weOpenedIt);
    bool gpioIsOpen() {return _gpioOpen;}
private:
    // At present there are only 7 available ports on a DSM
    static const PORT_DEFS MAX_PORT = PORT7;
    // String-ize config values
    static const char* STR_LOOPBACK;
    static const char* STR_RS232;
    static const char* STR_RS422;
    static const char* STR_RS485_HALF;
    static const char* STR_RS485_FULL;
    static const char* STR_NO_TERM;
    static const char* STR_TERM_120_OHM;
    static const char* STR_POWER_OFF;
    static const char* STR_POWER_ON;

    static const unsigned char LOOPBACK_BITS = 0b00000000;
    static const unsigned char RS232_BITS = 0b00000001;
    static const unsigned char RS485_HALF_BITS = 0b00000010;
    static const unsigned char RS422_RS485_BITS = 0b00000011;
    static const unsigned char TERM_120_OHM_BIT = 0b00000100;
    static const unsigned char SENSOR_POWER_ON_BIT = 0b00001000;

    // Aggregation of xcvr port knobs to twiddle
    XcvrConfig _xcvrConfig;
    // This is the current port configuration contained in the lowest nibble always
    unsigned char _rawXcvrConfig;
    // at present we're relying on bus address to locate the 
    // desired device.
    int _busAddr;
    int _deviceAddr;
    // This is the libftdi1 context of the USB device we're interested in using
    // to control the port types via GPIO.
    struct ftdi_context* _pContext;
    // keeps track of whether the FT4232H GPIO is open
    bool _gpioOpen;

    // never use default constructor, copy constructors, operator=
    SerialXcvrCtrl();
    SerialXcvrCtrl(const SerialXcvrCtrl& rRight);
    SerialXcvrCtrl(SerialXcvrCtrl& rRight);
    const SerialXcvrCtrl& operator=(const SerialXcvrCtrl& rRight);
    SerialXcvrCtrl& operator=(SerialXcvrCtrl& rRight);
};

}} // namespace { nidas namespace core {

#endif //NIDAS_CORE_SERIALXCVRCONTROL_H
