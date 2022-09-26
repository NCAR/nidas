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
#ifndef NIDAS_UTIL_SYSFSGPIO_H
#define NIDAS_UTIL_SYSFSGPIO_H

#include <fstream>

#include "GpioIF.h"
#include "ThreadSupport.h"


namespace nidas { namespace util {

enum RPI_GPIO_DIRECTION {RPI_GPIO_INPUT, RPI_GPIO_OUTPUT};
enum RPI_PWR_GPIO {RPI_PWR_SER_0=7, RPI_PWR_SER_1=8, RPI_PWR_SER_2=6, RPI_PWR_SER_3=5,
                   RPI_PWR_SER_4=13, RPI_PWR_SER_5=12, RPI_PWR_SER_6=16, RPI_PWR_SER_7=19,
                   RPI_PWR_28V=4, RPI_PWR_AUX=17, RPI_PWR_BANK1=23, RPI_PWR_BANK2=27, RPI_PWR_BTCON=21};

RPI_PWR_GPIO gpioPort2RpiGpio(GPIO_PORT_DEFS gpio);

/*
 *  Proc filesystem GPIO interface class for Rpi2
 */
class SysfsGpio : public GpioIF
{
public:
    /*
     *  Because multiple specializations may exist on a single SysfsGpio interface
     *  Sync selects one mutex per interface.
     *
     *  Specializations of SysfsGpio should use the Sync class to protect their operations on
     *  the interface which they are concerned.
     */
    class Sync : public Synchronized
    {
    public:
        Sync(SysfsGpio* me);
        ~Sync();

        Sync(const Sync& rRight) = delete;
        Sync& operator=(const Sync& rRight) = delete;

    private:
        static Cond _sysfsCondVar;
        SysfsGpio* _me;

    };

    SysfsGpio(RPI_PWR_GPIO rpiGPIO, RPI_GPIO_DIRECTION direction = RPI_GPIO_OUTPUT);
    virtual ~SysfsGpio();
    virtual bool ifaceFound();

    /*
     *  reads the pre-specified sysfs GPIO
     */
    virtual unsigned char read();

    /*
     *  writes the pre-specified sysfs GPIO
     */
    virtual void write(unsigned char pins);

private:
    RPI_PWR_GPIO _rpiGpio;
    bool _foundInterface;
    std::string _gpioValueFile;
    RPI_GPIO_DIRECTION _direction;
    unsigned char _shadow;
};


}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_SYSFSGPIO_H
