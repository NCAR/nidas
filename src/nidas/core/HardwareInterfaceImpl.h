/**
 * @file
 *
 * Types and interfaces helpful to HardwareInterface implementations.
 */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2023, Copyright University Corporation for Atmospheric Research
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
#ifndef _HARDWAREINTERFACEIMPL_H_
#define _HARDWAREINTERFACEIMPL_H_

#include "HardwareInterface.h"

#include <memory>

namespace nidas {
namespace core {

/**
 * Contain implementation back-end details for a HardwareDevice, like
 * providing a way to cache the pointers to implementation interfaces.
 */
class HardwareDeviceImpl
{
public:
    HardwareDeviceImpl(const HardwareDevice& device=HardwareDevice(),
                       const std::string& description="",
                       const std::string& path=""):
        _id(device.id()),
        _description(description),
        _path(path),
        _iserial(0),
        _ioutput(0),
        _ibutton(0)
    {}

    std::string _id;
    std::string _description;
    std::string _path;

    // These are smart pointers so they will be deleted when the device
    // implementation cache in the base HardwareInterface is destroyed.
    // Implementations therefore have to make sure to only cache dynamically
    // allocated implementations.  These are shared_ptr and not unique_ptr to
    // make it easy to copy the allocated interfaces into the cache map, but
    // really there will only ever be one reference once an interface for a
    // device is allocated.
    std::shared_ptr<SerialPortInterface> _iserial;
    std::shared_ptr<OutputInterface> _ioutput;
    std::shared_ptr<ButtonInterface> _ibutton;
};

} // namespace core
} // namespace nidas

#endif // _HARDWAREINTERFACEIMPL_H_
