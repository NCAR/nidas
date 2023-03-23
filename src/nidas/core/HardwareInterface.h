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
 * @brief Hardware devices have an identifier, description, and interfaces.
 *
 * A string identifier allows the hardware device namespace to be extended
 * without changing an enumeration, and it could enable more elaborate naming
 * schemes, like device paths or urls.  Since some hardware devices can
 * support multiple interfaces (eg serial ports have both transceivers and
 * power relays), sharing the device namespace across kinds of hardware
 * control allows the same device ID to be used to get multiple hardware
 * interfaces.  The id is a unique handle which identifies a specific hardware
 * component on a DSM.  Finally, the device IDs are frequently referenced as
 * strings for program and configuration arguments, so it is just simpler to
 * keep them as strings.  However, the symbol should always be used to refer
 * to standard DSM devices in code.
 *
 * HardwareDevice objects specifically are not virtual and are not meant to be
 * subclasses.  They are meant to passed around freely like tokens or
 * enumerations to identify a specific hardware component through which
 * callers can access one or more specific hardware interfaces.
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
     * Lookup the description for this device in the current implementation,
     * or else return the default description if there is no current
     * implementation.
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

private:
    std::string _id;
    std::shared_ptr<HardwareInterface> _hwi;
};


class HardwareDeviceImpl;
class HardwareDevices;


/**
 * @brief Standard DSM devices, including relays, ports, buttons, and LEDs.
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
    // interfaces.
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
     * case to compare against the standard lower-case IDs.  @p devices is the
     * vector of devices to search, typically selected from a list provided by
     * the HardwareInterface implementation, like relays() or ports().
     *
     * If the device is not found, then a default HardwareDevice is returned,
     * for which isEmpty() returns true.
     *
     * @param id device identifier
     * @param devices vector of devices to search
     * @return HardwareDevice 
     */
    virtual HardwareDevice
    lookupDevice(const std::string& id,
                 const std::vector<HardwareDevice>& devices);

    /**
     * @brief Like lookupDevice(), but search devices().
     *
     * @param id 
     * @return HardwareDevice 
     */
    virtual HardwareDevice
    lookupDevice(const std::string& id);

    static std::string
    lookupDescription(const HardwareDevice& device);

    /**
     * @brief Return all the standard devices in a vector.
     *
     * The base implementation returns all the standard DSM devices, but a
     * subclass could override it to add more if they are available for a
     * specific implementation.
     */
    virtual std::vector<HardwareDevice> devices();

    /**
     * @brief Return iterable of all serial port devices, PORT0...PORT7.
     *
     * These are both outputs and serial ports.
     * 
     * @return std::vector<HardwareDevice> 
     */
    virtual std::vector<HardwareDevice> ports();

    /**
     * @brief Return iterable of all relays: DCDC, BANK1, BANK2, AUX.
     * 
     * These are all outputs.
     * 
     * @return std::vector<HardwareDevice> 
     */
    virtual std::vector<HardwareDevice> relays();

    /**
     * @brief Return iterable of all buttons.
     *
     * These have button interfaces and also output interfaces for the
     * corresponding LED.
     *
     * @return std::vector<HardwareDevice> 
     */
    virtual std::vector<HardwareDevice> buttons();

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

    /**
     * @brief Port modes.
     */
    enum PORT_TYPES {
        LOOPBACK=0,
        RS232=232,
        RS422=422,
        RS485_FULL=422,
        RS485_HALF=484
    };

    /**
     * @brief Termination settings.
     */
    enum TERM {
        NO_TERM=0,
        TERM_120_OHM
    };

    SerialPortInterface();

    virtual
    ~SerialPortInterface();

    /**
     * Return the current port configuration in @p ptype and @p term.
     */
    virtual void
    getConfig(PORT_TYPES& ptype, TERM& term);

    /**
     * Set the port configuration in @p ptype and @p term.
     */
    virtual void
    setConfig(PORT_TYPES ptype, TERM term);

private:

    PORT_TYPES _port_type;
    TERM _termination;
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
    enum STATE { UNKNOWN, OFF, ON };

    OutputInterface();

    virtual
    ~OutputInterface();

    virtual STATE getState();

    virtual void setState(STATE state);

    /**
     * Base implementation calls setState(ON).
     */
    virtual void on();

    /**
     * Base implementation calls setState(OFF).
     */
    virtual void off();

    static std::string stateToString(STATE state);
    static STATE stringToState(const std::string& text);

private:
    STATE _current;
};


std::ostream&
operator<<(std::ostream& out, OutputInterface::STATE state);


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
