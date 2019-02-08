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
#ifndef NIDAS_UTIL_DSMPOWERCTRL_H
#define NIDAS_UTIL_DSMPOWERCTRL_H

#include "PowerGPIO.h"
#include "PowerCtrlAbs.h"

namespace nidas { namespace util {

enum DSM_POWER_IFACES
{
	PWR_DEVICE_ILLEGAL = -1,
	PWR_DEVICE_28V,		// any device can use this
	PWR_DEVICE_AUX,		// typically used by another DSM
	PWR_DEVICE_BANK1,	// turns off the serial panel power altogether (all sensors)
	PWR_DEVICE_BANK2,	// turns of the FTDI board  (TODO: This changes in next board turn!)
	NUM_PWR_DEVICES
};

const unsigned char PWR_BITS_28V = 0b00000001;
const unsigned char PWR_BITS_AUX = 0b00000010;
const unsigned char PWR_BITS_BANK1 = 0b00000100;
const unsigned char PWR_BITS_BANK2 = 0b00001000;
const char* PWR_IFACE_STR_ILLEGAL = "DSM_PWR_IFACE_ILLEGAL";
const char* PWR_IFACE_STR_28V =     "DSM_PWR_IFACE_28V";
const char* PWR_IFACE_STR_AUX =     "DSM_PWR_IFACE_AUX";
const char* PWR_IFACE_STR_BANK1 =   "DSM_PWR_IFACE_BANK1";
const char* PWR_IFACE_STR_BANK2 =   "DSM_PWR_IFACE_BANK2";

inline unsigned char pwrIface2bits(DSM_POWER_IFACES iface) {
	unsigned char retval = 0;

	switch (iface) {
	case PWR_DEVICE_28V:
		retval = PWR_BITS_28V;
		break;
	case PWR_DEVICE_AUX:
		retval = PWR_BITS_AUX;
		break;
	case PWR_DEVICE_BANK1:
		retval = PWR_BITS_BANK1;
		break;
	case PWR_DEVICE_BANK2:
		retval = PWR_BITS_BANK2;
		break;
	case PWR_DEVICE_ILLEGAL:
	default:
		DLOG(("pwrDevice2Bits(): Unknown power iface ID") << iface);
		break;
	}

	return retval;
}

inline std::string pwrIface2Str(DSM_POWER_IFACES iface) {
	std::string retval;

	switch (iface) {
	case PWR_DEVICE_28V:
		retval.append(PWR_IFACE_STR_28V);
		break;
	case PWR_DEVICE_AUX:
		retval.append(PWR_IFACE_STR_AUX);
		break;
	case PWR_DEVICE_BANK1:
		retval.append(PWR_IFACE_STR_BANK1);
		break;
	case PWR_DEVICE_BANK2:
		retval.append(PWR_IFACE_STR_BANK2);
		break;
	case PWR_DEVICE_ILLEGAL:
	default:
		retval.append(PWR_IFACE_STR_ILLEGAL);
		DLOG(("pwrDevice2Bits(): Unknown power iface ID") << iface);
		break;
	}

	return retval;
}

/*
 *  This class specializes PowerCtrlIF by providing a manual means to enable/disable power control
 */
class DSMPowerCtrl : public PowerGPIO, public PowerCtrlAbs
{
public:
    DSMPowerCtrl(DSM_POWER_IFACES device);
    virtual ~DSMPowerCtrl()
    {
        DLOG(("DSMPowerCtrl::~DSMPowerCtrl(): destructing..."));
    }

    virtual void pwrOn();
    virtual void pwrOff();
    virtual void print()
    {
        std::cout << "Power Device" << pwrIface2Str(getPwrIface()) << " ";
        PowerCtrlAbs::print();
    }

    DSM_POWER_IFACES getPwrIface() {return _iface;}

    std::string getPowerStateStr() {
        return rawPowerToStr(read());
    }
    void updatePowerState();

    // This utility converts a binary power configuration to a string
    const std::string rawPowerToStr(unsigned char powerCfg);
    // This utility converts a binary power configuration to state representation
    POWER_STATE rawPowerToState(unsigned char powerCfg);

private:
    DSM_POWER_IFACES _iface;
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_DSMPOWERCTRL_H
