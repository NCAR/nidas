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

#include <boost/regex.hpp>
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

namespace nidas { namespace util {

static const std::string PROCFS_CPUINFO = "/proc/cpuinfo";
static const boost::regex RPI2_DETECTOR("^Hardware[[:blank:]]+:[[:blank:]]+BCM");
static const std::ostringstream SYSFS_GPIO_ROOT_PATH("/sys/class/gpio");
static const int RPI_GPIO_MIN = 2;
static const int RPI_GPIO_MAX = 27;

Cond SysfsGpio::Sync::_sysfsCondVar;

/*
 *  Proc filesystem GPIO interface class for Rpi2
 */
SysfsGpio::SysfsGpio(int sysfsGPIO, RPI_GPIO_DIRECTION dir)
: _gpioN(sysfsGPIO), _foundInterface(false), _gpioFileStream(), _direction(dir)
{
    if (RANGE_CHECK_INC(RPI_GPIO_MIN, _gpioN, RPI_GPIO_MAX)) {
        std::ifstream cpuInfoStrm(PROCFS_CPUINFO.c_str());
        std::string cpuInfoBuf;

        // check that procfs is implemented...
        if (cpuInfoStrm.good()) {
            cpuInfoStrm.seekg(0, std::ios::end);
            cpuInfoBuf.reserve(cpuInfoStrm.tellg());
            cpuInfoStrm.seekg(0, std::ios::beg);

            cpuInfoBuf.assign((std::istreambuf_iterator<char>(cpuInfoStrm)),
                               std::istreambuf_iterator<char>());

            // check for Rpi...
            boost::cmatch results;
            bool isRaspberryPi = boost::regex_search(cpuInfoBuf.c_str(), results, RPI2_DETECTOR);

            if (isRaspberryPi) {
                std::ostringstream sysfsGpioN(SYSFS_GPIO_ROOT_PATH.str());
                sysfsGpioN << "/gpio" << _gpioN;

                // check to see if gpioN has been exported, and export if needed
                if (!boost::filesystem::exists(sysfsGpioN.str())) {
                    std::ostringstream sysfsGpioExport(SYSFS_GPIO_ROOT_PATH.str());
                    sysfsGpioExport << "/export";
                    std::ofstream exportStrm(sysfsGpioExport.str().c_str());

                    // Find the export executable?
                    if (exportStrm.good()) {
                        exportStrm << _gpioN;
                        bool gpioNExists = boost::filesystem::exists(sysfsGpioN.str());
                        for (int i=0; i<5 && !gpioNExists; ++i) {
                            sleep(1);
                            gpioNExists = boost::filesystem::exists(sysfsGpioN.str());
                        }

                        // did the export work?
                        if (gpioNExists) {
                            // check the group - should have gpio assigned by udev...
                            struct stat fInfo;
                            if (!stat(sysfsGpioN.str().c_str(), &fInfo)) {
                                struct group* pGroup = getgrgid(fInfo.st_gid);

                                // did udev set the gpioN file to the correct gpio group?
                                if (pGroup && std::string("gpio") == pGroup->gr_name) {

                                    // check group permissions
                                    boost::filesystem::file_status gpioFileStatus;
                                    gpioFileStatus.permissions(boost::filesystem::owner_read|boost::filesystem::owner_write
                                                          |boost::filesystem::group_read|boost::filesystem::group_write);

                                    // Are the gpioN file permissions set correctly?
                                    if (boost::filesystem::permissions_present(gpioFileStatus)) {
                                        _foundInterface = true;
                                    }
                                    else {
                                        DLOG(("SysfsGpio::SysfsGpio(): Interface not found: No group r/w access to GPIO") << _gpioN);
                                    }
                                }
                                else {
                                    DLOG(("SysfsGpio::SysfsGpio(): Interface not found: GPIO") << _gpioN << "is not in group: gpio.");
                                }
                            }
                            else {
                                DLOG(("SysfsGpio::SysfsGpio(): Interface not found: Could not stat sysfs for GPIO") << _gpioN);
                            }
                        }
                        else {
                            DLOG(("SysfsGpio::SysfsGpio(): Interface not found: Could not export GPIO") << _gpioN);
                        }
                    }
                }
                else {
                    _foundInterface = true;
                }
            }
            else {
                DLOG(("SysfsGpio::SysfsGpio(): Can't access /proc/cpuinfo..."));
            }
        }
        else {
            DLOG(("SysfsGpio::SysfsGpio(): requested GPIO is out of range: 2-27"));
        }
    }
    else {
        DLOG(("SysfsGpio::SysfsGpio(): System is not a RaspberryPi platform"));
    }

    if (ifaceFound()) {
        std::ostringstream gpioFile(SYSFS_GPIO_ROOT_PATH.str());
        gpioFile << "/gpio" << _gpioN;
        _gpioFileStream.open(gpioFile.str().c_str(), std::_S_in| std::_S_out);
        DLOG(("SysfsGpio::SysfsGpio(): Interface found: GPIO") << _gpioN);
    }
}

unsigned char SysfsGpio::read()
{
    unsigned char retval = 0xFF;
    if (_gpioFileStream.good()) {
        _gpioFileStream >> retval;
    }

    return retval;
}

void SysfsGpio::write(unsigned char pins)
{
    if (_gpioFileStream.good()) {
        _gpioFileStream << (pins ? "1" : "0");
    }
}


}} //namespace nidas { namespace util {

