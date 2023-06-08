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
#ifndef _HARDWAREINTERFACE_H_
#define _HARDWAREINTERFACE_H_

#include "PortType.h"

#include <string>
#include <vector>
#include <memory>

namespace nidas {
namespace core {

class SerialPortInterface;
class OutputInterface;
class ButtonInterface;
class HardwareInterface;


/**
 * Outputs can have one of three states: UNKNOWN, OFF, ON.
 */
struct OutputState
{
private:
    enum STATE {
        EUNKNOWN, EOFF, EON
    } id;

public:
    static const OutputState UNKNOWN;
    static const OutputState OFF;
    static const OutputState ON;

    /**
     * Return "on", "off", or "unknown".
     */
    std::string toString() const;

    /**
     * Set state if text is "on" or "off" and return true, otherwise return
     * false.
     */
    bool
    parse(const std::string& text);

    bool operator==(const OutputState& right)
    {
        return this->id == right.id;
    }

    OutputState(STATE state = EUNKNOWN):
        id(state)
    {}
};

std::ostream&
operator<<(std::ostream& out, const OutputState& state);


/**
 * @brief Hardware devices have an identifier, description, and interfaces.
 *
 * A string identifier allows the hardware device namespace to be extended
 * without changing an enumeration, and it could enable more elaborate naming
 * schemes, like device paths or urls.  Since some hardware devices can
 * support multiple interfaces (eg serial ports have both transceivers and
 * power relays), sharing the device namespace across kinds of hardware
 * control allows the same device ID to be used to get multiple hardware
 * interfaces.  The id is a unique handle which identifies a specific hardware
 * component on a DSM.  Device IDs are frequently referenced as strings for
 * program and configuration arguments, so it is just simpler to keep them as
 * strings.  However, the symbols should always be used to refer to standard
 * DSM devices in code.
 *
 * HardwareDevice objects specifically are not virtual and are not meant to be
 * subclassed.  They are meant to be used like tokens to identify a specific
 * hardware component through which callers can access one or more specific
 * hardware interfaces.  Once a HardwareDevice is bound to a HardwareInterface
 * implementation, either through HardwareDevice::lookupDevice() or by
 * requesting one of the interfaces through iSerial(), iOutput(), or iPort(),
 * then the HardwareDevice object holds a reference to the HardwareInterface.
 * The HardwareInterface will not be released (and hardware devices may be
 * held open) until the HardwareDevice goes out of scope or reset() is called.
 */
class HardwareDevice
{
public:
    HardwareDevice(const std::string& id="");

    /**
     * Return the string id for this device.  The id is unique for all
     * hardware devices, but the id is not unique for all interfaces, since a
     * single device may provide multiple interfaces.
     *
     * @return std::string 
     */
    std::string id() const;

    /**
     * Return the canonical device path, like /dev/ttyDSM0 for PORT0.
     *
     * Call lookupPath() on the current HardwareInterface implementation, or
     * else a standard default.  Like description(), this does not create and
     * hold the HardwareInterface pointer if it does not exist yet.
     */
    std::string path() const;

    /**
     * Lookup the description for this device in the current implementation,
     * or else return the default description if there is no current
     * implementation.  This does not hold a reference to the
     * HardwareInterface.
     *
     * @return std::string 
     */
    std::string description() const;

    /**
     * @brief Return true if this is an empty, unrecognized device.
     * 
     * @return true 
     * @return false 
     */
    bool isEmpty() const;

    /**
     * Get a pointer to the SerialPortInterface of this device, if available.
     *
     * This is the same as passing this device to
     * HardwareInterface::getSerialPortInterface().  It is not a const call
     * because this object keeps a shared pointer to the hardware interface.
     * The hardware interface will not be closed until reset() is called or
     * the instance is destroyed, thus it's important to limit the lifetime of
     * this object to avoid holding the hardware interfaces open longer than
     * needed.  The interface pointer is only valid during the lifetime of the
     * HardwareInterface implementation which owns it.
     *
     * Since the standard devices are const, they must be copied into a local
     * (scoped) instance to call the interfaces:
     *
     * @code
     * HardwareDevice port(Devices::PORT0);
     * if (auto ioutput = port.iOutput())
     *     ioutput->on();
     * @endcode
     *
     * There is no point creating and caching a HardwareInterface
     * implementation for a null device (isEmpty() returns true), so in that
     * case all the returned interface pointers are null and no
     * HardwareInterface implementation is created.
     *
     * @return SerialPortInterface* 
     */
    SerialPortInterface* iSerial();

    /**
     * Same as iSerial(), except for the OutputInterface.
     * 
     * @return OutputInterface* 
     */
    OutputInterface* iOutput();

    /**
     * Same is iSerial(), except for the ButtonInterface.
     * 
     * @return ButtonInterface* 
     */
    ButtonInterface* iButton();

#ifdef notdef
    /**
     * Return true if this device has an output interface.  Most
     * implementations will have to open the hardware to make sure the
     * interface is available.
     */
    bool isOutput();

    /**
     * Return true if this device is a serial port, meaning it has a
     * SerialPortInterface.  Most implementations will have to open the
     * hardware to make sure the interface is available.
     */
    bool isPort();

    /**
     * Return true if this device is a relay, meaning it has only an
     * OutputInterface and is not a button or port.  Most implementations will
     * have to open the hardware to check which interfaces are available.
     */
    bool isRelay();

    /**
     * Return true if this device is a button, meaning it has a
     * ButtonInterface.  Most implementations will have to open the hardware
     * to check which interfaces are available.
     */
    bool isButton();
#endif

    /**
     * Release any cached pointer to the HardwareInterface.
     *
     * This allows the hardware interface to be closed when all other
     * references are released, even if this device is not destroyed.  No
     * interface pointers from this device should be used after this call
     * without retrieving them again from the interface methods, in which case
     * the hardware interface will be cached again.
     */
    void reset();

    /**
     * Convenience method to return a power state for this device.
     * 
     * If the device does not have a power interface or the current power
     * state could not be retrieved, then return OutputInterface::UNKNOWN.
     */
    OutputState getOutputState();

    /**
     * Return a HardwareDevice for the given @p id.
     *
     * This gets a reference to the hardware implementation with
     * HardwareInterface::getHardwareInterface(), then calls lookupDevice()
     * with the given @p id.  The returned HardwareDevice will have a
     * reference to the HardwareInterface implementation, so the interface
     * will be valid until the returned device is destroyed or reset().  If no
     * such device is found for @p id, then the returned device still holds a
     * reference to the implementation, and isEmpty() will return true.
     *
     * Unless a reference to the HardwareInterface is held elsewhere, such as
     * by first calling HardwareInterface::getHardwareInterface(), then
     * looking up one device after another could cause the HardwareInterface
     * implementation to be created then destroyed for each lookup.  For now
     * this is an accepted trade-off, since it should not happen frequently.
     * Creating the interface does not (currently) require opening any
     * hardware.  Hardware is only accessed when a device interface is
     * requested.
     *
     * This method avoids unnecessarily creating a hardware interface
     * implementation for a device which has no known interfaces.  It first
     * looks up @p id in the default HardwareInterface, since that contains a
     * registry of all known devices.  If the device is not there, then this
     * returns an empty HardwareDevice, and all the interface methods return
     * null pointers without holding onto an implementation reference.
     */
    static HardwareDevice
    lookupDevice(const std::string& id);

    /**
     * Lookup a const HardwareDevice (eg PORT0 or DCDC) and return the non-const
     * copy with a reference to the hardare implementation.  This just calls
     * lookupDevice() on the id of @p device.
     */
    static HardwareDevice
    lookupDevice(const HardwareDevice& device);

    /**
     * Return true if this instance holds a reference to a HardwareInterface.
     */
    bool hasReference();

private:
    std::string _id;
    std::shared_ptr<HardwareInterface> _hwi;

    // Only HardwareInterface is allowed to bind itself to a HardwareDevice.
    HardwareDevice&
    bind(std::shared_ptr<HardwareInterface> hwi);

    HardwareInterface*
    get_hardware_interface();

    friend class HardwareInterface;
};


/**
 * Write the device id() to the stream.
 */
std::ostream&
operator<<(std::ostream& out, const HardwareDevice& device);


class HardwareDeviceImpl;
class HardwareDevices;


/**
 * @brief Standard DSM devices: relays, ports, buttons, LEDs.
 *
 * These are not grouped into the kinds of device, like ports or buttons,
 * because some devices have multiple control and query interfaces.
 *
 * On the DSM3, the button originally designated for the "default" setup
 * became known as DEF in the code, even though the silkscreen label on the
 * serial card says P1.  So symbols for both are defined here, but they refer
 * to the same device, with string id "p1".
 */
namespace Devices {
    // On DSM3, these serial ports have power and serial port interfaces.
    extern const HardwareDevice PORT0;
    extern const HardwareDevice PORT1;
    extern const HardwareDevice PORT2;
    extern const HardwareDevice PORT3;
    extern const HardwareDevice PORT4;
    extern const HardwareDevice PORT5;
    extern const HardwareDevice PORT6;
    extern const HardwareDevice PORT7;

    // On DSM3, these are power relays with only output interfaces.
    extern const HardwareDevice DCDC;
    extern const HardwareDevice BANK1;
    extern const HardwareDevice BANK2;
    extern const HardwareDevice AUX;

    // On DSM3, these are the button/LED pairs with both output and button
    // interfaces.  DEF is an alias for P1.
    extern const HardwareDevice P1;
    extern const HardwareDevice DEF;
    extern const HardwareDevice WIFI;
};


/**
 * @brief Methods to query and control DSM hardware.
 *
 * Interface to hardware devices on this system for which NIDAS can provide a
 * specific interface to query and control the hardware.  The hardware can be
 * power relays, serial port transceivers, pushbuttons, and LEDs.  Hardware
 * devices have IDs which are implemented as strings, and the ID is used to
 * lookup a hardware control interface for that specific device.
 *
 * All hardware access is through the HardwareInterface base class, but the
 * interface must be accessed through a static method rather than constructed,
 * in case the implementation uses a singleton.  The HardwareInterface can
 * return more specific interfaces for a given ID according to whether the
 * device exists on the system and supports the requested interface.  The
 * device-specific interfaces are OutputInterface for power relays and LEDs,
 * ButtonInterface for pushbuttons, and SerialPortInterface for configuring
 * serial port hardware like transceiver modes.
 *
 * If a returned interface pointer is null, then that interface is not
 * supported for that device on this system.
 *
 * On the DSM3 the sensor serial ports support both the OutputInterface and
 * the SerialPortInterface, one to control power to the sensor and the other
 * to configure the signalling mode of the serial port transceiver.  Each kind
 * of control is accessed through a different interface but with the same
 * device ID:
 *
 * @code {.C++}
 * if (auto iserial = Devices::PORT0.iSerial())
 *     iserial->setMode(SerialPortInterface::RS232);
 * if (auto ipower = Devices::PORT0.iOutput())
 *     ipower->On();
 * @endcode
 */
class HardwareInterface
{
public:
    /**
     * The path name for a HardwareInterface which returns no specific device
     * interfaces.
     */
    static const std::string NULL_INTERFACE;

    /**
     * The path name for a HardwareInterface implemented entirely in software.
     */
    static const std::string MOCK_INTERFACE;

    /**
     * @brief Return a HardwareInterface implementation.
     *
     * Return the default implementation for this system, or the specific
     * implementation previously passed to selectInterface().  If this system
     * has any detectable hardware to control, then the returned
     * HardwareInterface will be able to list the available devices and return
     * interfaces to query and control them.
     *
     * If a specific implementation was named and is not available, or if no
     * default implementation is available, then an interface is returned
     * which has no implementations, with the name "null".  This way callers
     * can always count on getting a non-null pointer value from this
     * function, even if there are no hardware devices available.
     *
     * The returned shared pointer determines the lifetime of the hardware
     * interface implementation.  Implementations should not be kept open any
     * longer than necessary.  Normally they can be tied to a scoped
     * HardwareDevice object or the HardwareInterface shared pointer.
     */
    static std::shared_ptr<HardwareInterface>
    getHardwareInterface();

    /**
     * @brief Select the default HardwareInterface implementation.
     *
     * @p path names a specific implementation which getHardwareInterface()
     * should try to return when called.  This has no effect if an
     * implementation has already been instantiated.  Pass an empty string to
     * reset to the default implementation, but only before an implementation
     * has been instantiated.
     *
     * If this is never called, then getHardwareInterface() returns a default
     * appropriate to the system.
     *
     * @param path 
     */
    static void
    selectInterface(const std::string& path);

    /**
     * @brief Release any current implementation and reset the default path.
     *
     * This cannot release an implementation which has already been returned
     * in a shared pointer and still has references.  This just ensures that
     * any subsequent call to getHardwareInterface() will return a new
     * implementation, which may or may not interfere with a previous
     * implementation.
     */
    static void
    resetInterface();

    /**
     * @brief Construct a HardwareInterface with no hardware controls.
     */
    HardwareInterface(const std::string& path);

    std::string
    getPath();

    virtual ~HardwareInterface();

    /**
     * Return a SerialPortInterface for the given @p device if one exists in
     * the current HardwareInterface implementation.  Usually this interface
     * should be accessed through the HardwareDevice::iSerial() method.  The
     * pointer lifetime is limited to the life of the HardwareInterface
     * implementation.
     */
    virtual SerialPortInterface*
    getSerialPortInterface(const HardwareDevice& device);

    /**
     * Return an OutputInterface for the given @p device if one exists in the
     * current HardwareInterface implementation.  Usually this interface
     * should be accessed through the HardwareDevice::iOutput() method.  The
     * pointer lifetime is limited to the life of the HardwareInterface
     * implementation.
     */
    virtual OutputInterface*
    getOutputInterface(const HardwareDevice& device);

    /**
     * Return a ButtonInterface for the given @p device if one exists in the
     * current HardwareInterface implementation.  Usually this interface
     * should be accessed through the HardwareDevice::iButton() method.  The
     * pointer lifetime is limited to the life of the HardwareInterface
     * implementation.
     */
    virtual ButtonInterface*
    getButtonInterface(const HardwareDevice& device);

    /**
     * @brief Lookup a hardware device by it's ID and return it.
     *
     * For backwards compatibility, the given id will be converted to lower
     * case to compare against the standard lower-case IDs.  Also, device
     * paths can be used to lookup the corresponding hardware device.  For
     * example, /dev/ttyDSM1 will return PORT1.
     *
     * The returned device will already be bound to this HardwareInterface.
     *
     * If the device is not found, then a default HardwareDevice is returned,
     * for which isEmpty() returns true.  Unlike for
     * HardwareDevice::lookupDevice(), where there may not already be a
     * HardwareInterface implementation, the returned empty HardwareDevice
     * still holds a reference to this instance, in case the caller intends to
     * use this device to keep the implementation alive without first having
     * to call getHardwareInterface() and hold that result.
     *
     * @param id device identifier
     * @return HardwareDevice 
     */
    virtual HardwareDevice
    lookupDevice(const std::string& id);

    /**
     * Lookup the id() of @p device to return a non-const HardwareDevice with
     * a reference to this HardwareInterface implementation.  See
     * lookupDevice(const std::string&).
     */
    HardwareDevice
    lookupDevice(const HardwareDevice& device);

    /**
     * Return the description for @p device.
     *
     * If the device is not a standard device or not known to the current
     * implementation, return an empty string.  This is static so that if an
     * implementation has not been created, then a standard description will
     * be returned without needing to create an implementation.
     */
    static std::string
    lookupDescription(const HardwareDevice& device);

    /**
     * Return the canonical device path for @p device.
     *
     * For example, this returns /dev/ttyDSM0 for DSM3 serial port
     * Devices::PORT0.  If the device is not a standard device or not known to
     * the current implementation, return an empty string.  Like for
     * lookupDescription(), this is static so that if an implementation has
     * not been created, then the path for a standard device will be returned
     * without needing to create an implementation.  If the
     * implementation-specific path is needed, make sure the implementation
     * exists first by holding the reference returned by
     * getHardwareInterface().
     */
    static std::string
    lookupPath(const HardwareDevice& device);

    /**
     * @brief Return the known devices for this HardwareInterface.
     *
     * The base implementation returns all the standard DSM devices, but a
     * subclass can override it to return a different set according to the
     * specific implementation.  The returned devices will already have a
     * reference to this HardwareInterface, so the HardwareInterface will not
     * be released until the returned vector or all of its elements go out of
     * scope.
     */
    virtual std::vector<HardwareDevice> devices();

protected:

    /**
     * Subclass implementations can cache HardwareDeviceImpl objects with the
     * base class so they can be returned automatically the next time an
     * interface is needed.  Also all cached devices will be deleted when the
     * HardwareInterface is destroyed.
     *
     * @param device 
     */
    void
    add_device_impl(const HardwareDeviceImpl& device);

    /**
     * Return a pointer to the cached HardwareDeviceImpl with the same id as
     * @p device, or else return null.  This method is *not* synchronized, the
     * HardwareInterface mutex must be locked *before* calling this method, so
     * that the returned pointer stays valid while the lock is held.
     *
     * @param device 
     * @return HardwareDeviceImpl* 
     */
    HardwareDeviceImpl*
    lookup_device_impl(const HardwareDevice& device);

    /**
     * Erase all cached devices.  Subclasses can use this when their
     * implementations should not outlive the subclass.
     */
    void
    delete_devices();

    virtual OutputInterface*
    createOutputInterface(HardwareDeviceImpl* dimpl);

    virtual SerialPortInterface*
    createSerialPortInterface(HardwareDeviceImpl* dimpl);

    virtual ButtonInterface*
    createButtonInterface(HardwareDeviceImpl* dimpl);

    std::string _path;

    /**
     * Use the pimpl pattern so the HardwareDeviceImpl definition is not
     * exposed.  The reference is for convenience.
     */
    std::unique_ptr<HardwareDevices> _devices_ptr;
    HardwareDevices& _devices;

};


/**
 * @brief Interface to query and control serial port hardware settings.
 */
class SerialPortInterface
{
public:

    SerialPortInterface();

    virtual
    ~SerialPortInterface();

    /**
     * Return the current port configuration in @p ptype and @p term.
     */
    virtual void
    getConfig(PortType& ptype, PortTermination& term);

    /**
     * Set the port configuration in @p ptype and @p term.
     */
    virtual void
    setConfig(PortType ptype, PortTermination term);

private:

    PortType _port_type;
    PortTermination _termination;
};


/**
 * @brief Interface for controlling outputs like power relays and LEDs.
 *
 * The base class is a null interface: it does nothing and returns UNKNOWN
 * state.
 */
class OutputInterface
{
public:
    OutputInterface();

    virtual
    ~OutputInterface();

    virtual OutputState getState();

    virtual void setState(OutputState state);

    /**
     * Base implementation calls setState(ON).
     */
    virtual void on();

    /**
     * Base implementation calls setState(OFF).
     */
    virtual void off();

    /**
     * Return true if getState() returns ON.
     */
    virtual bool isOn();

    /**
     * Return true if getState() returns OFF.
     */
    virtual bool isOff();

private:
    OutputState _current;
};




class ButtonInterface
{
public:
    enum STATE { UNKNOWN, UP, DOWN };

    ButtonInterface();

    virtual
    ~ButtonInterface();

    virtual STATE getState();

    /**
     * Base implementation returns true if getState() returns UP.
     */
    virtual bool isUp();

    /**
     * Base implementation returns true if getState() returns DOWN.
     */
    virtual bool isDown();

    /**
     * @brief Set the mock state when used as a mock interface.
     * 
     * @param state 
     */
    virtual void mockState(STATE state);

private:
    STATE _current;
};


} // namespace core
} // namespace nidas

#endif // _HARDWAREINTERFACE_H_
