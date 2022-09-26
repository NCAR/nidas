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

#include <string>

#include <regex>
#include <boost/filesystem.hpp>
#include <string>
#include <fcntl.h>
#include "pwd.h"
#include "grp.h"
#include "sys/stat.h"

#include "SysfsGpio.h"
#include "ThreadSupport.h"
#include "IOException.h"
#include "Logger.h"
#include "InvalidParameterException.h"
#include "util.h"

#include <map>

using namespace std;
namespace bf = boost::filesystem;

namespace nidas { namespace util {

static const std::string PROCFS_CPUINFO{"/proc/cpuinfo"};
static const std::string SYSFS_GPIO_ROOT_PATH{"/sys/class/gpio"};
static const int RPI_GPIO_MIN = 2;
static const int RPI_GPIO_MAX = 27;

Cond SysfsGpio::Sync::_sysfsCondVar;

/*
 *  Sysfs GPIO interface class for Rpi2
 */
SysfsGpio::SysfsGpio(RPI_PWR_GPIO rpiGPIO, RPI_GPIO_DIRECTION dir)
: _rpiGpio(rpiGPIO), _foundInterface(false),
  _gpioValueFile{},
  _direction(dir), _shadow(0)
{
    bool gpioNAlreadyExists = false;
    bool gpioNExported = false;

    DLOG(("SysfsGpio::SysfsGpio(): Checking range of rpiGPIO: ") << rpiGPIO);
    if (RANGE_CHECK_INC(RPI_GPIO_MIN, _rpiGpio, RPI_GPIO_MAX)) {
        DLOG(("SysfsGpio::SysfsGpio(): Try to open /proc/cpuinfo in an ifstream..."));
        std::ifstream cpuInfoStrm(PROCFS_CPUINFO.c_str());
        std::string cpuInfoBuf;

        // check that procfs is implemented...
        if (cpuInfoStrm.good()) {
            DLOG(("SysfsGpio::SysfsGpio(): Can open /proc/cpuinfo"));
            DLOG(("SysfsGpio::SysfsGpio(): Copying data out of /proc/cpuinfo into string buf"));
            cpuInfoBuf.assign((std::istreambuf_iterator<char>(cpuInfoStrm)),
                               std::istreambuf_iterator<char>());

            DLOG(("SysfsGpio::SysfsGpio(): Found the following in procfs cpuinfo: ") << cpuInfoBuf);

            // check for Rpi...
            DLOG(("SysfsGpio::SysfsGpio(): Checking for Raspberry Pi system..."));
            cmatch results;
            bool isRaspberryPi = false;
            try {
                static const std::string RPI2_DETECTOR_REGEX_SPEC("Hardware[[:blank:]]+:[[:blank:]]+BCM");
                static regex RPI2_DETECTOR_REGEX(RPI2_DETECTOR_REGEX_SPEC, std::regex_constants::extended);
                isRaspberryPi = regex_search(cpuInfoBuf.c_str(), results, RPI2_DETECTOR_REGEX);
            } catch (std::exception& e) {
                std::cerr << "SysfsGpio::SysfsGpio(): RPi regex check failed: " << e.what() << std::endl;
            }

            if (isRaspberryPi) {
                DLOG(("SysfsGpio::SysfsGpio(): Found Raspberry Pi system, checking to see if GPIO is already exported."));
                std::ostringstream sysfsGpioN(SYSFS_GPIO_ROOT_PATH);
                sysfsGpioN << "/gpio" << _rpiGpio;

                // check to see if gpioN has been exported, and export if needed
                if (!bf::exists(bf::path(sysfsGpioN.str().c_str()))) {
                    DLOG(("SysfsGpio::SysfsGpio(): GPIO is not exported - attempting export to: ") << sysfsGpioN.str());
                    std::ostringstream sysfsGpioExport(SYSFS_GPIO_ROOT_PATH);
                    sysfsGpioExport << "/export";
                    DLOG(("SysfsGpio::SysfsGpio(): Export to: ") << sysfsGpioExport.str());
                    std::ofstream exportStrm(sysfsGpioExport.str().c_str());

                    // Find the export executable?
                    if (exportStrm.good()) {
                        DLOG(("SysfsGpio::SysfsGpio(): Export file is opened successfully. Exporting GPIO ID ") << _rpiGpio);
                        exportStrm << _rpiGpio;
                        exportStrm.close();
                        sleep(1);
                        // We use symlink_status() because we know in advance that /sys/class/gpio/gpioN is a symbolic link.
                        bf::file_status gpioFileStatus = bf::symlink_status(bf::path(sysfsGpioN.str().c_str()));
                        gpioNExported = bf::is_symlink(gpioFileStatus);
                        DLOG(("SysfsGpio::SysfsGpio(): ") << sysfsGpioN.str() << (gpioNExported ? " exported." : " not exported yet."));
                        for (int i=0; i<10 && !gpioNAlreadyExists; ++i) {
                            sleep(1);
                            gpioFileStatus = bf::symlink_status(bf::path(sysfsGpioN.str().c_str()));
                            gpioNAlreadyExists = bf::is_symlink(gpioFileStatus);
                        }

                        // did the export work?
                        if (gpioNExported) {
                            DLOG(("SysfsGpio::SysfsGpio(): GPIO successfully exported. Checking group id..."));
                            // check the group - should have gpio assigned by udev...
                            struct stat fInfo;
                            if (!stat(sysfsGpioN.str().c_str(), &fInfo)) {
                                struct group* pGroup = getgrgid(fInfo.st_gid);

                                // did udev set the gpioN file to the correct gpio group?
                                if (pGroup && std::string("gpio") == pGroup->gr_name) {

                                    DLOG(("SysfsGpio::SysfsGpio(): gpio group ID checks out..."));
                                    // check group permissions
                                    bool groupPermsOk = gpioFileStatus.permissions()
                                                        && (bf::group_read|bf::group_write);

                                    // Are the gpioN file permissions set correctly?
                                    if (groupPermsOk) {
                                        DLOG(("SysfsGpio::SysfsGpio(): gpio group perms look good. Setting interface available."));
                                        _foundInterface = true;
                                    }
                                    else {
                                        DLOG(("SysfsGpio::SysfsGpio(): Interface not found: No group r/w access to GPIO") << _rpiGpio);
                                    }
                                }
                                else {
                                    DLOG(("SysfsGpio::SysfsGpio(): Interface not found: GPIO") << _rpiGpio << "is not in group: gpio.");
                                }
                            }
                            else {
                                DLOG(("SysfsGpio::SysfsGpio(): Interface not found: Could not stat sysfs for GPIO") << _rpiGpio);
                            }
                        }
                        else {
                            DLOG(("SysfsGpio::SysfsGpio(): Interface not found: Could not export GPIO") << _rpiGpio);
                        }
                    }
                }
                else {
                    DLOG(("SysfsGpio::SysfsGpio(): Interface already exported."));
                    gpioNAlreadyExists = true;
                }
            }
            else {
                DLOG(("SysfsGpio::SysfsGpio(): Raspberry Pi not detected..."));
            }
        }
        else {
            DLOG(("SysfsGpio::SysfsGpio(): requested GPIO is out of range: 2-27"));
        }
    }
    else {
        DLOG(("SysfsGpio::SysfsGpio(): System is not a RaspberryPi platform"));
    }

    std::fstream gpioFileStream;

    if (_foundInterface || gpioNAlreadyExists) {
        std::ostringstream gpioDirFile(SYSFS_GPIO_ROOT_PATH);
        gpioDirFile << "/gpio" << _rpiGpio << "/direction";
        DLOG(("SysfsGpio::SysfsGpio(): Attempting to open Rpi GPIO") << _rpiGpio << " direction control on this sysfs path: " << gpioDirFile.str());
        gpioFileStream.open(gpioDirFile.str().c_str(), std::_S_in| std::_S_out);
        if (gpioFileStream.good()) {
            DLOG(("SysfsGpio::SysfsGpio(): Successfully opened Rpi GPIO") << _rpiGpio << " direction control on this sysfs path: " << gpioDirFile.str());
            std::string dir((_direction == RPI_GPIO_OUTPUT ? "out" : "in"));
            if (gpioNExported) {
                DLOG(("SysfsGpio::SysfsGpio(): GPIO") << _rpiGpio << " was just exported, setting direction: " << dir);
                gpioFileStream << dir;
            }

            dir.clear();
            gpioFileStream >> dir;
            DLOG(("SysfsGpio::SysfsGpio(): Checking GPIO") << _rpiGpio << " direction: " << dir);
            gpioFileStream.close();
            if (((_direction == RPI_GPIO_OUTPUT) && (dir == "out"))
                 || ((_direction == RPI_GPIO_OUTPUT) && dir == "in")) {
                DLOG(("SysfsGpio::SysfsGpio(): GPIO") << _rpiGpio << " direction matches desired:" << dir);
            }
            else {
                DLOG(("SysfsGpio::SysfsGpio(): GPIO") << _rpiGpio << " direction DOES NOT match desired:" << dir);
                _foundInterface = false;
            }
        }
        else {
            _foundInterface = false;
            DLOG(("SysfsGpio::SysfsGpio(): Could not open gpio direction file:") << gpioDirFile.str());
        }

        std::ostringstream buf(SYSFS_GPIO_ROOT_PATH);
        buf << "/gpio" << _rpiGpio << "/value";
        _gpioValueFile = buf.str();
        DLOG(("SysfsGpio::SysfsGpio(): Attempting to open Rpi GPIO value file on this sysfs path: ") << _gpioValueFile);
        gpioFileStream.open(_gpioValueFile.c_str(), std::_S_in| std::_S_out);
        if (gpioFileStream.good()) {
            _foundInterface = true;
            DLOG(("SysfsGpio::SysfsGpio(): Interface found: GPIO") << _rpiGpio);
            std::string value;
            gpioFileStream >> value;
            DLOG(("SysfsGpio::SysfsGpio(): Current value of GPIO") << _rpiGpio << ": " << value);
        }
        else {
            _foundInterface = false;
            DLOG(("SysfsGpio::SysfsGpio(): Interface NOT found: GPIO") << _rpiGpio);
        }
        gpioFileStream.close();
    }
}

unsigned char SysfsGpio::read()
{
    unsigned char retval = _shadow;
    if (ifaceFound()) {
        DLOG(("SysfsGpio::read(): Reading gpio from ") << _gpioValueFile);
        std::fstream gpioFileStream(_gpioValueFile.c_str(), std::ios_base::in);
        if (gpioFileStream.good()) {
            gpioFileStream.seekg(0, std::ios::beg);
            gpioFileStream >> retval;
            _shadow = retval;
            DLOG(("SysfsGpio::read(): Value read from") << _gpioValueFile << " is " << (char)retval);
            gpioFileStream.close();
        }
    }
    else {
        DLOG(("SysfsGpio::read(): No interface to read."));
    }
    return retval;
}

void SysfsGpio::write(unsigned char pins)
{
    if (ifaceFound()) {
        std::fstream gpioFileStream(_gpioValueFile.c_str(), std::ios_base::out|std::ios_base::app);
        if (gpioFileStream.good()) {
            gpioFileStream << pins;
            DLOG(("SysfsGpio::write(): Value written to ") << _gpioValueFile << " is " << (char)pins);
            gpioFileStream.close();
        }
    }
    _shadow = pins;
}


RPI_PWR_GPIO gpioPort2RpiGpio(GPIO_PORT_DEFS gpio)
{
    static std::map<GPIO_PORT_DEFS, RPI_PWR_GPIO> names
    {
        { SER_PORT0, RPI_PWR_SER_0 },
        { SER_PORT1, RPI_PWR_SER_1 },
        { SER_PORT2, RPI_PWR_SER_2 },
        { SER_PORT3, RPI_PWR_SER_3 },
        { SER_PORT4, RPI_PWR_SER_4 },
        { SER_PORT5, RPI_PWR_SER_5 },
        { SER_PORT6, RPI_PWR_SER_6 },
        { SER_PORT7, RPI_PWR_SER_7 },
        { PWR_28V, RPI_PWR_28V },
        { PWR_AUX, RPI_PWR_AUX },
        { PWR_BANK1, RPI_PWR_BANK1 },
        { PWR_BANK2, RPI_PWR_BANK2 },
    };

    auto iter = names.find(gpio);
    if (iter != names.end())
        return iter->second;
    DLOG(("gpioPort2RpiPwrGpio(): unknown GPIO_PORT_DEFS value: ") << gpio);
    return static_cast<RPI_PWR_GPIO>(-1);
}


SysfsGpio::Sync::Sync(SysfsGpio* me):
    Synchronized(Sync::_sysfsCondVar),
    _me(me)
{
    DLOG(("Synced on SysfsGpio"));
}

SysfsGpio::Sync::~Sync()
{
    DLOG(("Sync released on SysfsGpio"));
    _me = 0;
}

SysfsGpio::~SysfsGpio()
{}

bool SysfsGpio::ifaceFound()
{
    return _foundInterface;
}


}} //namespace nidas { namespace util {

