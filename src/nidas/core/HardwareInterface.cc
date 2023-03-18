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

#include "HardwareInterface.h"
#include <algorithm>
#include <memory>


namespace {

    using namespace nidas::core;

    SerialPortInterface noop_serial_interface;
    OutputInterface noop_output_interface;
    ButtonInterface noop_button_interface;

    std::string default_interface_path;

    std::unique_ptr<HardwareInterface> hardware_interface_singleton;
}



namespace nidas {
namespace core {

const HardwareDevice Devices::DCDC{"dcdc", "DC-DC converter relay"};
const HardwareDevice Devices::BANK1{"bank1",
    "Bank1 12V to serial card and IO panel, not connected on DSM3"};
const HardwareDevice Devices::BANK2{"bank2",
    "Bank2 12V socket, for accessories."};
const HardwareDevice Devices::AUX{"aux",
    "Auxiliary 12V power, typically chained to other DSMs"};
const HardwareDevice Devices::PORT0{"port0",
    "Sensor power on DSM serial port 0"};
const HardwareDevice Devices::PORT1{"port1",
    "Sensor power on DSM serial port 1"};
const HardwareDevice Devices::PORT2{"port2",
    "Sensor power on DSM serial port 2"};
const HardwareDevice Devices::PORT3{"port3",
    "Sensor power on DSM serial port 3"};
const HardwareDevice Devices::PORT4{"port4",
    "Sensor power on DSM serial port 4"};
const HardwareDevice Devices::PORT5{"port5",
    "Sensor power on DSM serial port 5"};
const HardwareDevice Devices::PORT6{"port6",
    "Sensor power on DSM serial port 6"};
const HardwareDevice Devices::PORT7{"port7",
    "Sensor power on DSM serial port 7"};
const HardwareDevice Devices::P1{"p1",
    "p1 button and LED, also known as default switch."};
const HardwareDevice Devices::WIFI{"wifi", "wifi button and LED."};
const HardwareDevice Devices::DEF{P1};


std::vector<HardwareDevice> HardwareInterface::ports()
{
    return std::vector<HardwareDevice>{
        Devices::PORT0,
        Devices::PORT1,
        Devices::PORT2,
        Devices::PORT3,
        Devices::PORT4,
        Devices::PORT5,
        Devices::PORT6,
        Devices::PORT7
    };
}

std::vector<HardwareDevice> HardwareInterface::relays()
{
    return std::vector<HardwareDevice>{
        Devices::DCDC,
        Devices::BANK1,
        Devices::BANK2,
        Devices::AUX
    };
}

std::vector<HardwareDevice> HardwareInterface::buttons()
{
    return std::vector<HardwareDevice>{
        Devices::P1,
        Devices::WIFI
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
HardwareDevice(const std::string& id,
               const std::string& description):
    _id(id),
    _description(description)
{}

std::string
HardwareDevice::id() const
{
    return _id;
}

std::string
HardwareDevice::description() const
{
    return _description;
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
    _path(path)
{
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
HardwareInterface::getSerialPortInterface(const HardwareDevice&)
{
    return nullptr;
}

OutputInterface*
HardwareInterface::getOutputInterface(const HardwareDevice&)
{
    return nullptr;
}

ButtonInterface*
HardwareInterface::getButtonInterface(const HardwareDevice&)
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
    _current = ON;
}

void
OutputInterface::
off()
{
    _current = OFF;
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


SerialPortInterface::PORT_TYPES
SerialPortInterface::
getMode()
{
    return _port_type;
}

SerialPortInterface::TERM
SerialPortInterface::
getTermination()
{
    return _termination;
}

bool
SerialPortInterface::
setMode(PORT_TYPES mode)
{
    _port_type = mode;
    return true;
}

bool
SerialPortInterface::
setTermination(TERM term)
{
    _termination = term;
    return true;
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
        HardwareInterface("mock")
    {}

    SerialPortInterface*
    getSerialPortInterface(const HardwareDevice& device) override
    {
        if (lookupDevice(device.id(), ports()).isEmpty())
            return nullptr;
        return &noop_serial_interface;
    }

    OutputInterface*
    getOutputInterface(const HardwareDevice& device) override
    {
        if (lookupDevice(device.id(), ports()).isEmpty() &&
            lookupDevice(device.id(), relays()).isEmpty() &&
            lookupDevice(device.id(), buttons()).isEmpty())
            return nullptr;
        return &noop_output_interface;
    }

    ButtonInterface*
    getButtonInterface(const HardwareDevice& device) override
    {
        if (lookupDevice(device.id(), buttons()).isEmpty())
            return nullptr;
        return &noop_button_interface;
    }

};


HardwareInterface*
HardwareInterface::getHardwareInterface()
{
    if (!hardware_interface_singleton)
    {
        // if mock requested, then return it.
        if (default_interface_path == "mock")
        {
            hardware_interface_singleton.reset(new MockHardwareInterface());
        }
    }
    return hardware_interface_singleton.get();
}


} // namespace core
} // namespace nidas
