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
#include <list>
#include <iostream>

#include <nidas/util/Logger.h>
#include <nidas/util/ThreadSupport.h>


using namespace nidas::core;
using nidas::util::Mutex;
using nidas::util::Synchronized;


namespace {

Mutex hardware_interface_mutex;

std::string default_interface_path;

// This is just a cache pointer so the same interface can be returned if
// an implementation is already available.  The lifetime of the
// implementation is tied to the lifetime of the caller's shared pointer
// and the hardware device objects that cache it.
std::weak_ptr<HardwareInterface> hardware_interface_singleton;

// Create a default HardwareInterface attached to a static local shared
// pointer.  This can be used to lookup standard device information for which
// a full HardwareInterface implementation is not needed.  It returns a shared
// pointer, and the result is not meant to be kept.  The default
// implementation will be released when static local objects are destroyed.
auto
get_default_interface()
{
    static auto default_interface = std::make_shared<HardwareInterface>("");
    return default_interface;
}

}



namespace nidas {
namespace core {

const OutputState OutputState::UNKNOWN(EUNKNOWN);
const OutputState OutputState::OFF(EOFF);
const OutputState OutputState::ON(EON);

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


namespace DSM3
{
    const HardwareDevice OUTPUTS[] {
        DCDC, BANK1, BANK2, AUX,
        PORT0, PORT1, PORT2, PORT3, PORT4, PORT5, PORT6, PORT7,
        P1, WIFI
    };

    const HardwareDevice RELAYS[] {
        DCDC, BANK1, BANK2, AUX
    };

    const HardwareDevice PORTS[] {
        PORT0, PORT1, PORT2, PORT3, PORT4, PORT5, PORT6, PORT7,
    };

    const HardwareDevice BUTTONS[] {
        P1, WIFI
    };
}



/**
 * Search an iterable of devices for the given id and return it.  This is a
 * template so the iterable can be an array type with a length.
 */
template <typename T>
HardwareDevice lookup_device(const std::string& id, const T& arrayd)
{
    for (auto& dp: arrayd)
    {
        if (dp.id() == id)
            return dp;
    }
    return HardwareDevice();
}



class HardwareDevices: public std::list<HardwareDeviceImpl>
{
public:
    std::string
    to_string()
    {
        std::ostringstream out;
        for (auto& device: *this)
        {
            out << "{" << device._id << ","
                << device._description << "}";
        }
        return out.str();
    }
};


std::vector<HardwareDevice> HardwareInterface::devices()
{
    Synchronized sync(hardware_interface_mutex);
    // create a vector of devices bound to this interface.
    std::vector<HardwareDevice> devs;
    for (auto& dimpl: _devices)
    {
        auto hwi = hardware_interface_singleton.lock();
        devs.push_back(HardwareDevice(dimpl._id).bind(hwi));
    }
    return devs;
}


HardwareDevice::
HardwareDevice(const std::string& id):
    _id(id),
    _hwi()
{}


bool
HardwareDevice::
hasReference()
{
    return bool(_hwi);
}


HardwareDevice&
HardwareDevice::
bind(std::shared_ptr<HardwareInterface> hwi)
{
    _hwi = hwi;
    return *this;
}

HardwareInterface*
HardwareDevice::
get_hardware_interface()
{
    if (isEmpty())
        return nullptr;
    if (!_hwi)
    {
        _hwi = HardwareInterface::getHardwareInterface();
    }
    return _hwi.get();
}

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

std::string
HardwareDevice::path() const
{
    return HardwareInterface::lookupPath(*this);
}

OutputState
HardwareDevice::
getOutputState()
{
    if (auto ioutput = iOutput())
        return ioutput->getState();
    return OutputState::UNKNOWN;
}


bool
HardwareDevice::
isEmpty() const
{
    return _id.empty();
}


HardwareDevice
HardwareDevice::
lookupDevice(const std::string& id)
{
    // As a short cut, to avoid creating a hardware interface just to look up
    // a device we know won't exist, first check if the device exists in the
    // default.
    if (get_default_interface()->lookupDevice(id).isEmpty())
    {
        return HardwareDevice();
    }
    auto hwi = HardwareInterface::getHardwareInterface();
    return hwi->lookupDevice(id);
}


HardwareDevice
HardwareDevice::
lookupDevice(const HardwareDevice& device)
{
    return lookupDevice(device.id());
}


std::ostream&
operator<<(std::ostream& out, const HardwareDevice& device)
{
    return (out << device.id());
}


SerialPortInterface*
HardwareDevice::iSerial()
{
    auto hwi = get_hardware_interface();
    return !hwi ? nullptr : hwi->getSerialPortInterface(*this);
}

OutputInterface*
HardwareDevice::iOutput()
{
    auto hwi = get_hardware_interface();
    return !hwi ? nullptr : hwi->getOutputInterface(*this);
}

ButtonInterface*
HardwareDevice::iButton()
{
    auto hwi = get_hardware_interface();
    return !hwi ? nullptr : hwi->getButtonInterface(*this);
}

void
HardwareDevice::reset()
{
    _hwi.reset();
}


HardwareInterface::
HardwareInterface(const std::string& path):
    _path(path),
    _devices_ptr(new HardwareDevices),
    _devices(*_devices_ptr)
{
    using HDI = HardwareDeviceImpl;
    add_device_impl(
        HDI(DCDC, "DC-DC converter relay"));
    add_device_impl(
        HDI(BANK1, "Bank1 12V to serial card and IO panel, "
                   "not connected on DSM3"));
    add_device_impl(
        HDI(BANK2, "Bank2 12V socket, for accessories."));
    add_device_impl(
        HDI(AUX, "Auxiliary 12V power, typically chained to other DSMs"));
    add_device_impl(
        HDI(PORT0, "Sensor power on DSM serial port 0", "/dev/ttyDSM0"));
    add_device_impl(
        HDI(PORT1, "Sensor power on DSM serial port 1", "/dev/ttyDSM1"));
    add_device_impl(
        HDI(PORT2, "Sensor power on DSM serial port 2", "/dev/ttyDSM2"));
    add_device_impl(
        HDI(PORT3, "Sensor power on DSM serial port 3", "/dev/ttyDSM3"));
    add_device_impl(
        HDI(PORT4, "Sensor power on DSM serial port 4", "/dev/ttyDSM4"));
    add_device_impl(
        HDI(PORT5, "Sensor power on DSM serial port 5", "/dev/ttyDSM5"));
    add_device_impl(
        HDI(PORT6, "Sensor power on DSM serial port 6", "/dev/ttyDSM6"));
    add_device_impl(
        HDI(PORT7, "Sensor power on DSM serial port 7", "/dev/ttyDSM7"));
    add_device_impl(
        HDI(P1, "p1 button and LED, also known as default switch."));
    add_device_impl(
        HDI(WIFI, "wifi button and LED."));
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
    // We don't really care if there is already an implementation with a
    // different path, just set the path for the next time an implementation
    // will be loaded.
    Synchronized sync(hardware_interface_mutex);
    default_interface_path = path;
}


void
HardwareInterface::
resetInterface()
{
    // if an interface has already been returned to a caller, then that
    // instance will remain even if the singleton reference is reset.
    // however, whether that other instance interferes with a new one depends
    // on the implementations.
    Synchronized sync(hardware_interface_mutex);
    hardware_interface_singleton.reset();
    default_interface_path = "";
}


SerialPortInterface*
HardwareInterface::getSerialPortInterface(const HardwareDevice& device)
{
    Synchronized sync(hardware_interface_mutex);
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
    Synchronized sync(hardware_interface_mutex);
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
    Synchronized sync(hardware_interface_mutex);
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
lookupDevice(const HardwareDevice& device)
{
    return lookupDevice(device.id());
}


HardwareDevice
HardwareInterface::
lookupDevice(const std::string& target)
{
    DLOG(("") << "lookupDevice(" << target << ")");
    std::string id(target);
    // translate device names like ttyDSM0 to the hardware id port0
    auto n = id.rfind("ttyDSM");
    if (n != std::string::npos)
    {
        auto port = id.substr(n);
        DLOG(("") << "found port device: " << port);
        if (port.size() == 7 && port.back() >= '0' && port.back() <= '7')
        {
            id = port.back();
        }
    }
    std::for_each(id.begin(), id.end(), [](char &c){ c = std::tolower(c); });
    if (id.size() == 1 && id[0] >= '0' && id[0] <= '7')
        id = "port" + id;
    // "def" is an alias for p1, but we have to check for the string id
    // because the symbol DEF already has the same id as P1.
    if (id == "def")
        id = P1.id();
    DLOG(("") << "searching for resolved id: " << id);
    for (auto& device: devices())
    {
        if (device.id() == id)
            return device;
    }
    return HardwareDevice();
}



std::string
HardwareInterface::
lookupDescription(const HardwareDevice& device)
{
    static auto default_interface = get_default_interface();
    Synchronized sync(hardware_interface_mutex);
    auto hwi = hardware_interface_singleton.lock();
    if (!hwi)
        hwi = default_interface;
    if (auto dimpl = hwi->lookup_device_impl(device))
        return dimpl->_description;
    return "";
}


std::string
HardwareInterface::
lookupPath(const HardwareDevice& device)
{
    static auto default_interface = get_default_interface();
    Synchronized sync(hardware_interface_mutex);
    auto hwi = hardware_interface_singleton.lock();
    if (!hwi)
        hwi = default_interface;
    if (auto dimpl = hwi->lookup_device_impl(device))
        return dimpl->_path;
    return "";
}


void
HardwareInterface::
add_device_impl(const HardwareDeviceImpl& device)
{
    Synchronized sync(hardware_interface_mutex);
    VLOG(("") << "adding " << device._id << ": " << device._description);
    _devices.push_back(device);
}


HardwareDeviceImpl*
HardwareInterface::
lookup_device_impl(const HardwareDevice& device)
{
    for (auto& dimpl: _devices)
    {
        if (dimpl._id == device.id())
            return &dimpl;
    }
    return nullptr;
}


void
HardwareInterface::
delete_devices()
{
    Synchronized sync(hardware_interface_mutex);
    _devices.clear();
}


OutputInterface::OutputInterface() :
    _current()
{}

OutputInterface::
~OutputInterface()
{}


OutputState
OutputInterface::
getState()
{
    return _current;
}

void
OutputInterface::
on()
{
    setState(OutputState::ON);
}

void
OutputInterface::
off()
{
    setState(OutputState::OFF);
}

void
OutputInterface::
setState(OutputState state)
{
    _current = state;
}

std::string
OutputState::
toString() const
{
    if (id == EON)
        return "on";
    else if (id == EOFF)
        return "off";
    return "unknown";
}


bool
OutputState::
parse(const std::string& text)
{
    if (text == "on" || text == "ON")
        id = EON;
    else if (text == "off" || text == "OFF")
        id = EOFF;
    else
        return false;
    return true;
}


std::ostream&
operator<<(std::ostream& out, const OutputState& state)
{
    out << state.toString();
    return out;
}


const PortType PortType::LOOPBACK(ELOOPBACK);
const PortType PortType::RS232(ERS232);
const PortType PortType::RS422(ERS422);
const PortType PortType::RS485_FULL(ERS485_FULL);
const PortType PortType::RS485_HALF(ERS485_HALF);


struct port_type_aliases_t
{
    PortType ptype;
    std::vector<std::string> aliases;
};


// Associate port types with names.  The first alias is the short form, the
// last is the long form.
std::vector<port_type_aliases_t> port_type_aliases{
    { PortType::LOOPBACK,    { "loop", "loopback", "LOOPBACK" } },
    { PortType::RS232,       { "232", "rs232", "RS232" } },
    { PortType::RS422,       { "422", "rs422", "RS422" } },
    { PortType::RS485_FULL,  { "485f", "rs485f", "485F", "rs485_full", "RS485_FULL" } },
    { PortType::RS485_HALF,  { "485h", "rs485h", "485H", "rs485_half", "RS485_HALF" } }
};


std::string
PortType::
toShortString() const
{
    for (auto& pa: port_type_aliases)
    {
        if (pa.ptype == *this)
            return pa.aliases.front();
    }
    // something is really wrong...
    return "undefined";
}


std::string
PortType::
toLongString() const
{
    for (auto& pa: port_type_aliases)
    {
        if (pa.ptype == *this)
            return pa.aliases.back();
    }
    // shouldn't happen...
    return "undefined";
}


bool
PortType::
parse(const std::string& text)
{
    for (auto& pa: port_type_aliases)
    {
        for (auto& alias: pa.aliases)
        {
            if (text == alias)
            {
                ptype = pa.ptype.ptype;
                return true;
            }
        }
    }
    return false;
}


std::ostream&
operator<<(std::ostream& out, const PortType& ptype)
{
    out << ptype.toShortString();
    return out;
}


const PortTermination PortTermination::NO_TERM(ENO_TERM);
const PortTermination PortTermination::TERM_ON(ETERM_ON);


std::string
PortTermination::
toShortString() const
{
    if (term == ETERM_ON)
        return "term";
    return "noterm";
}


std::string
PortTermination::
toLongString() const
{
    if (term == ETERM_ON)
        return "TERM_ON";
    return "NO_TERM";
}


bool
PortTermination::
parse(const std::string& text)
{
    if (text == "term" || text == "TERM_ON" || text == "TERM")
    {
        term = ETERM_ON;
    }
    else if (text == "noterm" || text == "NO_TERM")
    {
        term = ENO_TERM;
    }
    else
    {
        return false;
    }
    return true;
}


std::ostream&
operator<<(std::ostream& out, const PortTermination& pterm)
{
    out << pterm.toShortString();
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
    _port_type(PortType::RS232),
    _termination(PortTermination::NO_TERM)
{
}


SerialPortInterface::
~SerialPortInterface()
{}


void
SerialPortInterface::
getConfig(PortType& ptype, PortTermination& term)
{
    ptype = _port_type;
    term = _termination;
}


void
SerialPortInterface::
setConfig(PortType ptype, PortTermination term)
{
    _port_type = ptype;
    _termination = term;
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
        if (lookup_device(dimpl->_id, DSM3::PORTS).isEmpty())
            return nullptr;
        return new SerialPortInterface;
    }

    OutputInterface*
    createOutputInterface(HardwareDeviceImpl* dimpl) override
    {
        if (lookup_device(dimpl->_id, DSM3::OUTPUTS).isEmpty())
            return nullptr;
        return new OutputInterface;
    }

    ButtonInterface*
    createButtonInterface(HardwareDeviceImpl* dimpl) override
    {
        if (lookup_device(dimpl->_id, DSM3::BUTTONS).isEmpty())
            return nullptr;
        return new ButtonInterface;
    }

};


extern HardwareInterface* get_hardware_interface_ftdi();


std::shared_ptr<HardwareInterface>
HardwareInterface::getHardwareInterface()
{
    auto hwi = hardware_interface_singleton.lock();
    if (!hwi)
    {
        // if mock requested, then return it.
        if (default_interface_path == MOCK_INTERFACE)
        {
            hwi = std::make_shared<MockHardwareInterface>();
        }
        else if (default_interface_path == "ftdi")
        {
            hwi.reset(get_hardware_interface_ftdi());
        }
        else if (default_interface_path == "")
        {
            // really there is only one real implementation, so try that one
            // if nothing else specified.
            hwi.reset(get_hardware_interface_ftdi());
        }
        else
        {
            // always set the implementation to something, even if a null
            // implementation, so there is only ever one attempt to find the
            // default implementation.
            hwi = std::make_shared<HardwareInterface>(NULL_INTERFACE);
        }
        hardware_interface_singleton = hwi;
    }
    return hwi;
}


} // namespace core
} // namespace nidas
