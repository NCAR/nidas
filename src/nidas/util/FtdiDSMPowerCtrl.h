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
#ifndef NIDAS_UTIL_FTDIDSMPOWERCTRL_H
#define NIDAS_UTIL_FTDIDSMPOWERCTRL_H

#include "FtdiDSMPowerGPIO.h"
#include "PowerCtrlAbs.h"

namespace nidas { namespace util {

// TODO: These change to bits 0-3 when the second iteration of the new board arrives.
const unsigned char PWR_BITS_28V =   0b00001000;
const unsigned char PWR_BITS_AUX =   0b00010000;
const unsigned char PWR_BITS_BANK1 = 0b00100000;
const unsigned char PWR_BITS_BANK2 = 0b01000000;

inline unsigned char pwrGpio2bits(GPIO_PORT_DEFS gpio) {
	unsigned char retval = 0;

	switch (gpio) {
	case PWR_28V:
		retval = PWR_BITS_28V;
		break;
	case PWR_AUX:
		retval = PWR_BITS_AUX;
		break;
	case PWR_BANK1:
		retval = PWR_BITS_BANK1;
		break;
	case PWR_BANK2:
		retval = PWR_BITS_BANK2;
		break;
	case ILLEGAL_PORT:
	default:
		DLOG(("pwrGpio2Bits(): Unknown power gpio ID") << gpio);
		break;
	}

	return retval;
}

/*
 *  This class specializes PowerCtrlIF by providing a manual means to enable/disable power control
 */
class FtdiDSMPowerCtrl : public FtdiDSMPowerGPIO, public PowerCtrlAbs
{
public:
    FtdiDSMPowerCtrl(GPIO_PORT_DEFS gpio);
    virtual ~FtdiDSMPowerCtrl()
    {
        DLOG(("FtdiDSMPowerCtrl::~FtdiDSMPowerCtrl(): destructing..."));
    }

    virtual void pwrOn();
    virtual void pwrOff();
    virtual void print()
    {
        std::cout << gpio2Str(getPwrPort()) << " ";
        PowerCtrlAbs::print();
    }

    bool ifaceAvailable() {return FtdiDSMPowerGPIO::ifaceFound(); }

    GPIO_PORT_DEFS getPwrPort() {return _pwrPort;}

    std::string getPowerStateStr() {
        return rawPowerToStr(read());
    }
    void updatePowerState();

    // This utility converts a binary power configuration to a string
    const std::string rawPowerToStr(unsigned char powerCfg);
    // This utility converts a binary power configuration to state representation
    POWER_STATE rawPowerToState(unsigned char powerCfg);

private:
    GPIO_PORT_DEFS _pwrPort;
    const unsigned char _pwrBit;
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_FTDIDSMPOWERCTRL_H
