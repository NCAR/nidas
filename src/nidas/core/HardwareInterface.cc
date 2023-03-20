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

#include "HardwareInterfaceImpl.h"
#include <algorithm>
#include <memory>
#include <map>
#include <iostream>

#include <nidas/util/Logger.h>

namespace {

    using namespace nidas::core;

    std::string default_interface_path;

    std::unique_ptr<HardwareInterface> hardware_interface_singleton;
}



namespace nidas {
namespace core {

const std::string HardwareInterface::NULL_INTERFACE("null");
const std::string HardwareInterface::MOCK_INTERFACE("mock");

namespace Devices
{
    const HardwareDevice DCDC("dcdc");
    const HardwareDevice BANK1("bank1");
    const HardwareDevice BANK2("bank2");
    const HardwareDevice AUX("aux");
    const HardwareDevice PORT0("port0");
    const HardwareDevice PORT1("port1");
    const HardwareDevice PORT2("port2");
    const HardwareDevice PORT3("port3");
    const HardwareDevice PORT4("port4");
    const HardwareDevice PORT5("port5");
    const HardwareDevice PORT6("port6");
    const HardwareDevice PORT7("port7");
    const HardwareDevice P1("p1");
    const HardwareDevice WIFI("wifi");
    const HardwareDevice DEF(P1);
}

using namespace Devices;


class HardwareDeviceMap: public std::map<std::string, HardwareDeviceImpl>
{
public:
    std::string
    to_string()
    {
        std::ostringstream out;
        for (auto& pairs: *this)
        {
            out << pairs.first << ": (" << pairs.second._id << ","
                << pairs.second._description << ");";
        }
        return out.str();
    }
};


std::vector<HardwareDevice> HardwareInterface::ports()
{
    return std::vector<HardwareDevice>{
        PORT0, PORT1, PORT2, PORT3,
        PORT4, PORT5, PORT6, PORT7
    };
}

std::vector<HardwareDevice> HardwareInterface::relays()
{
    return std::vector<HardwareDevice>{
        DCDC, BANK1, BANK2, AUX
    };
}

std::vector<HardwareDevice> HardwareInterface::buttons()
{
    return std::vector<HardwareDevice>{
        P1, WIFI
    };
}

template <typename T>
std::vector<T>& append(std::vector<T>& dest, const std::vector<T>& src)
{
    dest.insert(dest.end(), src.begin(), src.end());
    return dest;
}

std::vector<HardwareDevice> HardwareInterface::devices()
{
    std::vector<HardwareDevice> all;
    append(all, ports());
    append(all, relays());
    return append(all, buttons());
}


HardwareDevice::
HardwareDevice(const std::string& id):
    _id(id)
{}

std::string
HardwareDevice::id() const
{
    return _id;
}

std::string
HardwareDevice::description() const
{
    return HardwareInterface::lookupDescription(*this);
}

bool
HardwareDevice::
isEmpty() const
{
    return _id.empty();
}

SerialPortInterface*
HardwareDevice::iSerial() const
{
    auto hwi = HardwareInterface::getHardwareInterface();
    return !hwi ? nullptr : hwi->getSerialPortInterface(*this);
}

OutputInterface*
HardwareDevice::iOutput() const
{
    auto hwi = HardwareInterface::getHardwareInterface();
    return !hwi ? nullptr : hwi->getOutputInterface(*this);
}

ButtonInterface*
HardwareDevice::iButton() const
{
    auto hwi = HardwareInterface::getHardwareInterface();
    return !hwi ? nullptr : hwi->getButtonInterface(*this);
}


HardwareInterface::
HardwareInterface(const std::string& path):
    _path(path),
    _devices_ptr(new HardwareDeviceMap),
    _devices(*_devices_ptr)
{
    add_device_impl(HardwareDeviceImpl(DCDC, "DC-DC converter relay"));
    add_device_impl(HardwareDeviceImpl(BANK1, "Bank1 12V to serial card and IO panel, not connected on DSM3"));
    add_device_impl(HardwareDeviceImpl(BANK2, "Bank2 12V socket, for accessories."));
    add_device_impl(HardwareDeviceImpl(AUX, "Auxiliary 12V power, typically chained to other DSMs"));
    add_device_impl(HardwareDeviceImpl(PORT0, "Sensor power on DSM serial port 0"));
    add_device_impl(HardwareDeviceImpl(PORT1, "Sensor power on DSM serial port 1"));
    add_device_impl(HardwareDeviceImpl(PORT2, "Sensor power on DSM serial port 2"));
    add_device_impl(HardwareDeviceImpl(PORT3, "Sensor power on DSM serial port 3"));
    add_device_impl(HardwareDeviceImpl(PORT4, "Sensor power on DSM serial port 4"));
    add_device_impl(HardwareDeviceImpl(PORT5, "Sensor power on DSM serial port 5"));
    add_device_impl(HardwareDeviceImpl(PORT6, "Sensor power on DSM serial port 6"));
    add_device_impl(HardwareDeviceImpl(PORT7, "Sensor power on DSM serial port 7"));
    add_device_impl(HardwareDeviceImpl(P1, "p1 button and LED, also known as default switch."));
    add_device_impl(HardwareDeviceImpl(WIFI, "wifi button and LED."));
}


std::string
HardwareInterface::
getPath()
{
    return _path;
}


HardwareInterface::
~HardwareInterface()
{}


void
HardwareInterface::
selectInterface(const std::string& path)
{
    if (!hardware_interface_singleton.get())
    {
        default_interface_path = path;
    }
}


void
HardwareInterface::
resetInterface()
{
    hardware_interface_singleton.reset(nullptr);
    default_interface_path = "";
}


SerialPortInterface*
HardwareInterface::getSerialPortInterface(const HardwareDevice& device)
{
    HardwareDeviceImpl* dimpl = lookup_device_impl(device);
    // If this is not already a known device, meaning it's a "standard" device
    // or one the implementation has already added, then don't bother.
    if (!dimpl)
        return nullptr;
    // return an existing interface
    if (dimpl->_iserial)
    {
        return dimpl->_iserial.get();
    }
    // otherwise ask the implementation to create one.
    if (auto iserial = createSerialPortInterface(dimpl))
    {
        dimpl->_iserial.reset(iserial);
    }
    return dimpl->_iserial.get();
}


OutputInterface*
HardwareInterface::getOutputInterface(const HardwareDevice& device)
{
    HardwareDeviceImpl* dimpl = lookup_device_impl(device);
    // If this is not already a known device, meaning it's a "standard" device
    // or one the implementation has already added, then don't bother.
    if (!dimpl)
        return nullptr;
    // return an existing interface
    if (dimpl->_ioutput)
    {
        return dimpl->_ioutput.get();
    }
    // otherwise ask the implementation to create one.
    if (auto ioutput = createOutputInterface(dimpl))
    {
        dimpl->_ioutput.reset(ioutput);
    }
    return dimpl->_ioutput.get();
}


ButtonInterface*
HardwareInterface::getButtonInterface(const HardwareDevice& device)
{
    HardwareDeviceImpl* dimpl = lookup_device_impl(device);
    // If this is not already a known device, meaning it's a "standard" device
    // or one the implementation has already added, then don't bother.
    if (!dimpl)
        return nullptr;
    // return an existing interface
    if (dimpl->_ibutton)
    {
        return dimpl->_ibutton.get();
    }
    // otherwise ask the implementation to create one.
    if (auto ibutton = createButtonInterface(dimpl))
    {
        dimpl->_ibutton.reset(ibutton);
    }
    return dimpl->_ibutton.get();
}


OutputInterface*
HardwareInterface::
createOutputInterface(HardwareDeviceImpl*)
{
    return nullptr;
}

SerialPortInterface*
HardwareInterface::
createSerialPortInterface(HardwareDeviceImpl*)
{
    return nullptr;
}

ButtonInterface*
HardwareInterface::
createButtonInterface(HardwareDeviceImpl*)
{
    return nullptr;
}


HardwareDevice
HardwareInterface::
lookupDevice(const std::string& id, const std::vector<HardwareDevice>& devs)
{
    std::string lower(id);
    std::for_each(lower.begin(), lower.end(),
                  [](char &c){ c = std::tolower(c); });
    if (id.size() == 1 && id[0] >= '0' && id[0] <= '7')
        lower = "port" + lower;
    for (auto device: devs)
    {
        if (device.id() == lower)
            return device;
    }
    return HardwareDevice();
}


HardwareDevice
HardwareInterface::
lookupDevice(const std::string& id)
{
    return lookupDevice(id, devices());
}


std::string
HardwareInterface::
lookupDescription(const HardwareDevice& device)
{
    static HardwareInterface default_interface("");

    auto hwi = hardware_interface_singleton.get();
    if (!hwi)
        hwi = &default_interface;
    if (auto dimpl = hwi->lookup_device_impl(device))
        return dimpl->_description;
    return "";
}


void
HardwareInterface::
add_device_impl(const HardwareDeviceImpl& device)
{
    DLOG(("") << "adding " << device._id << ": " << device._description);
    _devices[device._id] = device;
    VLOG(("current device impls: ") << _devices.to_string());
}


HardwareDeviceImpl*
HardwareInterface::
lookup_device_impl(const HardwareDevice& device)
{
    auto it = _devices.find(device.id());
    if (it != _devices.end())
        return &(it->second);
    return nullptr;
}



OutputInterface::OutputInterface() :
    _current(STATE::UNKNOWN)
{}

OutputInterface::
~OutputInterface()
{}


OutputInterface::STATE
OutputInterface::
getState()
{
    return _current;
}

void
OutputInterface::
on()
{
    setState(ON);
}

void
OutputInterface::
off()
{
    setState(OFF);
}

void
OutputInterface::
setState(STATE state)
{
    _current = state;
}

std::string
OutputInterface::
stateToString(STATE state)
{
    if (state == ON)
        return "on";
    else if (state == OFF)
        return "off";
    return "unknown";
}


OutputInterface::STATE
OutputInterface::
stringToState(const std::string& text)
{
    if (text == "on")
        return ON;
    else if (text == "off")
        return OFF;
    return UNKNOWN;
}


std::ostream&
operator<<(std::ostream& out, OutputInterface::STATE state)
{
    out << OutputInterface::stateToString(state);
    return out;
}


ButtonInterface::
ButtonInterface():
    _current(UNKNOWN)
{}

ButtonInterface::
~ButtonInterface()
{}


bool
ButtonInterface::
isUp()
{
    return getState() == UP;
}


bool
ButtonInterface::
isDown()
{
    return getState() == DOWN;
}


void
ButtonInterface::
mockState(STATE state)
{
    _current = state;
}


ButtonInterface::STATE
ButtonInterface::
getState()
{
    return _current;
}


SerialPortInterface::
SerialPortInterface():
    _port_type(RS232),
    _termination(NO_TERM)
{
}


SerialPortInterface::
~SerialPortInterface()
{}


void
SerialPortInterface::
getConfig(PORT_TYPES* ptype, TERM* term)
{
    if (ptype)
        *ptype = _port_type;
    if (term)
        *term = _termination;
}


void
SerialPortInterface::
setConfig(PORT_TYPES* ptype, TERM* term)
{
    if (ptype)
        _port_type = *ptype;
    if (term)
        _termination = *term;
}


/**
 * @brief A mock HardwareInterface.
 *
 * This is just like HardwareInterface except it returns mock implementations
 * of the device-specific interfaces.  It mimics the interfaces that are
 * available for DSM3 devices.
 */
class MockHardwareInterface: public HardwareInterface
{
public:
    MockHardwareInterface():
        HardwareInterface(HardwareInterface::MOCK_INTERFACE)
    {}

    SerialPortInterface*
    createSerialPortInterface(HardwareDeviceImpl* dimpl) override
    {
        if (lookupDevice(dimpl->_id, ports()).isEmpty())
            return nullptr;
        return new SerialPortInterface;
    }

    OutputInterface*
    createOutputInterface(HardwareDeviceImpl* dimpl) override
    {
        if (lookupDevice(dimpl->_id, ports()).isEmpty() &&
            lookupDevice(dimpl->_id, relays()).isEmpty() &&
            lookupDevice(dimpl->_id, buttons()).isEmpty())
            return nullptr;
        return new OutputInterface;
    }

    ButtonInterface*
    createButtonInterface(HardwareDeviceImpl* dimpl) override
    {
        if (lookupDevice(dimpl->_id, buttons()).isEmpty())
            return nullptr;
        return new ButtonInterface;
    }

};


extern HardwareInterface* get_hardware_interface_ftdi();


HardwareInterface*
HardwareInterface::getHardwareInterface()
{
    if (!hardware_interface_singleton)
    {
        HardwareInterface* hwi = 0;
        // if mock requested, then return it.
        if (default_interface_path == MOCK_INTERFACE)
        {
            hwi = new MockHardwareInterface();
        }
        else if (default_interface_path == "ftdi")
        {
            hwi = get_hardware_interface_ftdi();
        }
        else
        {
            // always set the implementation to something, even if a null
            // implementation, so there is only ever one attempt to find the
            // default implementation.
            hwi = new HardwareInterface(NULL_INTERFACE);
        }
        hardware_interface_singleton.reset(hwi);
    }
    return hardware_interface_singleton.get();
}


} // namespace core
} // namespace nidas
