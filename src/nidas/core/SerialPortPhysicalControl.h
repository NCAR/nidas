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
#ifndef NIDAS_CORE_SERIALPORTCONTROL_H
#define NIDAS_CORE_SERIALPORTCONTROL_H

#include "ftdi.h"

namespace n_u = nidas::util;
namespace nidas { namespace core {

/**
 * types of serial ports
 */
typedef enum {LOOPBACK=0, RS232=232, RS422=422, RS485_FULL=485, RS485_HALF=484} PORT_TYPES;

/*
 * This enum specifies the ports in the DSM.
 */
typedef enum {PORT0=0, PORT1, PORT2, PORT3, PORT4, PORT5, PORT6, PORT7} PORT_DEFS;
/*
 * Serial termination settings for RS422/RS485
 */
typedef enum {NO_TERM=0, TERM_120_OHM} TERM;

/*
 * Sensor power setting
 */
typedef enum {SENSOR_POWER_OFF, SENSOR_POWER_ON} SENSOR_POWER_STATE;

/*
 ********************************************************************
 ** SerialPortPhysicalControl is a class which controls the Exar SP339 serial 
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
class SerialPortPhysicalControl {
public:
    // Constructor needs to know what port is being controlled
    // Constructor uses portID to decide which FTDI interface to 
    // use to control the port type.
    // Bus address is the means by which the USB device is opened.
    // So default them to the values known today, but may be 
    // overridden later.
    SerialPortPhysicalControl(const PORT_DEFS portId);
    SerialPortPhysicalControl(const PORT_DEFS portId, const PORT_TYPES portType, const TERM termination=NO_TERM, 
                              const SENSOR_POWER_STATE powerState=SENSOR_POWER_ON);
    // Destructor
    ~SerialPortPhysicalControl();

    // This sets the class state to be used by applyPortConfig();
    void setPortConfig(const PORT_TYPES portType, const TERM term, const SENSOR_POWER_STATE powerState);
    // This is the primary client API that does all the heavy lifting  
    // to actually change the SP339 driver port type/mode (RS232, RS422, etc).
    void applyPortConfig(const bool openDevice=true);
    // Returns the current state of the port mode, including sensor power
    unsigned char getPortConfig();
    // This informs the class as to which USB device to open.
    // This has no effect until the device is closed and then re-opened
    void setBusAddress(const int busId=1, const int deviceId=6);
    // This utility converts a PORT_TYPE to a string
    static const std::string portTypeToStr(const PORT_TYPES portType);
    // This utility converts a string to a PORT_TYPE
    static PORT_TYPES strToPortType(const char* portStr);
    // This utility converts a binary term configuration to a string
    const std::string termToStr(unsigned char termCfg) const; 
    // This utility converts a binary power configuration to a string
    const std::string powerToStr(unsigned char powerCfg) const; 
    // This utility prints the port types for a particular port.
    void printPortConfig(const PORT_DEFS port, const bool addNewline=true, const bool readFirst=true);
    // This is a utility to convert an integer to a PORT_DEFS port ID
    // Currently assumes that the portNum is in the range of PORT_DEFS
    static PORT_DEFS int2PortDef(const unsigned portNum)
    {
        return static_cast<PORT_DEFS>(portNum);
    }

protected:
    // shift the port type into the correct location in the GPIO for insertion
    unsigned char adjustBitPosition(const PORT_DEFS port, const unsigned char bits );
    // assembles the port config bits into a low nibble, ready for shifting.
    unsigned char assembleBits(const PORT_TYPES portType, const TERM term, const SENSOR_POWER_STATE powerState);
    // deduces the FT4232H GPIO port form the port passed in.
    // need to select the interface based on the specified port 
    // at present, assume 4 bits per port definition
    enum ftdi_interface port2iface(const unsigned int port);
    // Morphs PORT_TYPES to the SP339 M0/M1 bit definitions
    unsigned char portType2Bits(const PORT_TYPES portType);
    // Morphs the SP339 M0/M1 bit definitions to the associated PORT_TYPE 
    PORT_TYPES bits2PortType(const unsigned char bits);

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
    

    // The port this instance is tasked with managing (0-7)
    PORT_DEFS _portID;
    // This is the port type
    PORT_TYPES _portType;
    // This is the termination
    TERM _term;
    // This is the state of the sensor power - ON/OFF
    SENSOR_POWER_STATE _powerstate;
    // This is the current port configuration contained in the lowest nibble always
    unsigned char _portConfig;
    // at present we're relying on bus address to locate the 
    // desired device.
    int _busAddr;
    int _deviceAddr;
    // This is the libftdi1 context of the USB device we're interested in using
    // to control the port types via GPIO.
    struct ftdi_context* _pContext;

    // never use default constructor, copy constructors, operator=
    SerialPortPhysicalControl();
    SerialPortPhysicalControl(const SerialPortPhysicalControl& rRight);
    SerialPortPhysicalControl(SerialPortPhysicalControl& rRight);
    const SerialPortPhysicalControl& operator=(const SerialPortPhysicalControl& rRight);
    SerialPortPhysicalControl& operator=(SerialPortPhysicalControl& rRight);
};

}} // namespace { nidas namespace core {

#endif //NIDAS_CORE_SERIALPORTCONTROL_H
