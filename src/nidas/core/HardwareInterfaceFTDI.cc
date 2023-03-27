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
#include "nidas/util/Logger.h"
#include "nidas/util/ThreadSupport.h"

#include <ftdi.h>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::core::Devices;

using nidas::util::Mutex;
using nidas::util::Synchronized;



namespace {

std::string
iface_to_string(ftdi_interface iface)
{
    static std::map<ftdi_interface, std::string> strings{
        { INTERFACE_A, "A" },
        { INTERFACE_B, "B" },
        { INTERFACE_C, "C" },
        { INTERFACE_D, "D" },
        { INTERFACE_ANY, "ANY" }
    };
    auto it = strings.find(iface);
    if (it != strings.end())
        return it->second;
    return "UNKNOWN";
}


const std::string MANUFACTURER_UCAR{"UCAR"};

const std::string PRODUCT_GPIO{"GPIO"};
const std::string PRODUCT_I2C{"I2C"};
const std::string PRODUCT_P0_P3{"P0-P3"};
const std::string PRODUCT_P4_P7{"P4-P7"};

enum InterfaceType { OUTPUT, PORT, BUTTON };

/**
 * Each DSM3 FTDI device is controlled through one of the interfaces of a
 * specfici chip, with specific bits on that interface.  The mask must be
 * combined with the specific bits, eg LED output must and'ed with the button
 * device mask to get the actual bit for LED output.  And for serial ports,
 * SENSOR_BITS_POWER must be and'ed with the mask for the sensor power output.
 * The pin directions must be set accordingly for the output and input pins.
 */
struct ftdi_location
{
    HardwareDevice device;
    InterfaceType itype;
    const std::string product;
    enum ftdi_interface iface;
    unsigned char mask;
};


/**
 * Every serial port sensor gets half of an ftdi interface on the GPIO chip.
 * Port 0 is the low 4 bits of interface A, and port 1 is the high 4 bits, and
 * so on up to port 7 as the high 4 bits of interface D.  These masks pick off
 * the particular bitfields for power and port config from the 4-bit masks.
 * They are and'ed with the ftdi_location mask to get the specific bits.
 */
const unsigned char XCVR_BITS_PORT_TYPE = 0b00110011;
const unsigned char XCVR_BITS_TERM =      0b01000100;
const unsigned char PORT_CONFIG_BITS = (XCVR_BITS_PORT_TYPE | XCVR_BITS_TERM);
const unsigned char SENSOR_POWER_BITS =   0b10001000;

/**
 * These are the specific bits on INTERFACE_C of FTDI chip I2C to control
 * output on the corresponding relays.
 */
const unsigned char PWR_BITS_DCDC =  0b00000001;
const unsigned char PWR_BITS_AUX =   0b00000010;
const unsigned char PWR_BITS_BANK2 = 0b00000100;
const unsigned char PWR_BITS_BANK1 = 0b00001000;

// outgoing GPIO for LED indicators on bits 6 and 7,
// incoming GPIO for switch inputs come in on bits 4 & 5
// outgoing GPIO for power board control on bits 0-3
const unsigned char WIFI_BUTTON_BITS = 0b00010000;
const unsigned char P1_BUTTON_BITS   = 0b00100000;
const unsigned char WIFI_LED_BITS    = 0b01000000;
const unsigned char P1_LED_BITS      = 0b10000000;

/**
 * Return the information needed to address the specific bits for the given
 * device.  The mask should be and'ed with the bits above to get the
 * particular bit to set (output) or read (input).  Return nullptr the device
 * is not recognized.
 */
const ftdi_location*
get_ftdi_address(const HardwareDevice& device, InterfaceType itype)
{
    static std::vector<ftdi_location> locations
    {
        { PORT0, PORT, PRODUCT_GPIO, INTERFACE_A, 0b00001111 & PORT_CONFIG_BITS },
        { PORT1, PORT, PRODUCT_GPIO, INTERFACE_A, 0b11110000 & PORT_CONFIG_BITS },
        { PORT2, PORT, PRODUCT_GPIO, INTERFACE_B, 0b00001111 & PORT_CONFIG_BITS },
        { PORT3, PORT, PRODUCT_GPIO, INTERFACE_B, 0b11110000 & PORT_CONFIG_BITS },
        { PORT4, PORT, PRODUCT_GPIO, INTERFACE_C, 0b00001111 & PORT_CONFIG_BITS },
        { PORT5, PORT, PRODUCT_GPIO, INTERFACE_C, 0b11110000 & PORT_CONFIG_BITS },
        { PORT6, PORT, PRODUCT_GPIO, INTERFACE_D, 0b00001111 & PORT_CONFIG_BITS },
        { PORT7, PORT, PRODUCT_GPIO, INTERFACE_D, 0b11110000 & PORT_CONFIG_BITS },
        { PORT0, OUTPUT, PRODUCT_GPIO, INTERFACE_A, 0b00001111 & SENSOR_POWER_BITS },
        { PORT1, OUTPUT, PRODUCT_GPIO, INTERFACE_A, 0b11110000 & SENSOR_POWER_BITS },
        { PORT2, OUTPUT, PRODUCT_GPIO, INTERFACE_B, 0b00001111 & SENSOR_POWER_BITS },
        { PORT3, OUTPUT, PRODUCT_GPIO, INTERFACE_B, 0b11110000 & SENSOR_POWER_BITS },
        { PORT4, OUTPUT, PRODUCT_GPIO, INTERFACE_C, 0b00001111 & SENSOR_POWER_BITS },
        { PORT5, OUTPUT, PRODUCT_GPIO, INTERFACE_C, 0b11110000 & SENSOR_POWER_BITS },
        { PORT6, OUTPUT, PRODUCT_GPIO, INTERFACE_D, 0b00001111 & SENSOR_POWER_BITS },
        { PORT7, OUTPUT, PRODUCT_GPIO, INTERFACE_D, 0b11110000 & SENSOR_POWER_BITS },
        { DCDC, OUTPUT, PRODUCT_I2C, INTERFACE_C, PWR_BITS_DCDC },
        { AUX, OUTPUT, PRODUCT_I2C, INTERFACE_C, PWR_BITS_AUX },
        { BANK1, OUTPUT, PRODUCT_I2C, INTERFACE_C, PWR_BITS_BANK1 },
        { BANK2, OUTPUT, PRODUCT_I2C, INTERFACE_C, PWR_BITS_BANK2 },
        { WIFI, OUTPUT, PRODUCT_I2C, INTERFACE_C, WIFI_LED_BITS },
        { P1, OUTPUT, PRODUCT_I2C, INTERFACE_C, P1_LED_BITS },
        { WIFI, BUTTON, PRODUCT_I2C, INTERFACE_C, WIFI_BUTTON_BITS },
        { P1, BUTTON, PRODUCT_I2C, INTERFACE_C, P1_BUTTON_BITS }
    };
    for (auto& lp : locations)
    {
        if (lp.device.id() == device.id() && lp.itype == itype)
            return &lp;
    }
    return nullptr;
}


} // namespace anonymous


namespace nidas {
namespace core {

/**
 * FTDI_Device wraps libftdi to control specific bits on one interface.
 *
 * This manages specific bits on one FTDI USB device, specified as one of the
 * four interfaces of a FTDI chip identified by the manufacturer and product
 * strings.  libftdi can only create one context for an interface at a time,
 * so multiple devices (different bit fields) on the same interface have to
 * share a pointer to the context and synchronize access.  The synchronization
 * is also required to implement read-modify-write transactions on the
 * bitfields without interfering with each other.  Technically the two
 * synchronizations could be done with separate mutexes, but really there
 * should rarely be contention and no harm if a modification on one interface
 * has to wait for the other.
 *
 * This is based on the original FtdiGpio<DEVICE, IFACE> template
 * implementation, but it is simplified by not using templates and not
 * exposing the use of shared or weak pointers.
 */
class FTDI_Device
{
public:
    /**
     * Mutex to synchronize context and device sharing.
     */
    static Mutex ftdi_device_mutex;

    /**
     * Keep track of open FTDI_Device instances to know when a context can be
     * closed.
     */
    static std::vector<FTDI_Device*> ftdi_devices;

    /**
     * Create and open the FTDI_Device instance for the given device, or else
     * return null if it fails.  If this device only wants a subset of the
     * bits from ftdi_location, then it passes them in submask.  In
     * particular, this allows serial ports to mask the 4 GPIO bits for each
     * serial port into the output power bit and serial port config bits.
     */
    static std::unique_ptr<FTDI_Device>
    create(const HardwareDevice& device, InterfaceType itype);

    /**
     * Return a pointer to a ftdi_context for the USB interface @p iface on
     * the chip which matches the given @p product and @p manufacturer
     * strings. the FTDI device, returning false if the open fails.  If that
     * context does not exist yet, create it and open it and set the reference
     * count to 1.  Otherwise just increment the reference count.  If the open
     * fails, return null.
     */
    static bool
    open_interface(FTDI_Device* device);

    /**
     * Decrement the reference count for the given @p context, and if zero
     * close it and release it.
     */
    static void
    close_interface(FTDI_Device* device);

    /**
     * The bits which will be controlled by this adapter.  All other (output)
     * bits on the interface are preserved.  The mask is used to compute how
     * many bits to shift to get the target bits.
     *
     * @param mask 
     */
    void
    set_mask(unsigned char mask);

    void
    write_bits(unsigned char bits);

    bool
    read_bits(unsigned char* bits);

    std::string
    description();

   ~FTDI_Device();

 private:

    /**
     * Setup but do not open() a FTDI_Device for @p iface on the USB device
     * with the given @p manufacturer and @p product.  The specific bits to be
     * managed on the interface must be set with set_mask().
     *
     * This is private since all instances should be created by create().
     */
    FTDI_Device(enum ftdi_interface iface, const std::string& product,
                const std::string& manufacturer=MANUFACTURER_UCAR);

    bool
    open();

    void
    close();

    /** 
     * read_bits() without locking first and without shifting.
     */
    bool
    _read_bits(unsigned char* pins);

    /**
     * Format an ftdi error message, including the given context, the
     * description of this FTDI_DSM object, the error string from the ftdi
     * library, and the status return code.
     *
     * @param context 
     */
    std::string
    error_string(const std::string& context="", int status=0);

    FTDI_Device(const FTDI_Device&) = delete;
    FTDI_Device& operator=(const FTDI_Device&) = delete;

    ftdi_context* _pContext;
    ftdi_interface _iface;
    std::string _manufacturer;
    std::string _product;
    unsigned char _pinDirection;
    unsigned char _mask;
    unsigned int _shift;
};


Mutex FTDI_Device::ftdi_device_mutex;
std::vector<FTDI_Device*> FTDI_Device::ftdi_devices;


std::string
FTDI_Device::
description()
{
    std::ostringstream out;
    out << "ftdi(" << _product + "," << iface_to_string(_iface);
    out << ",pindir=0x" << std::hex << std::setw(2) << std::setfill('0')
        << (int)_pinDirection
        << ",mask=0x"  << std::hex << std::setw(2) << std::setfill('0')
        << (int)_mask
        << ",shift=" << std::dec << std::setw(0) << _shift << ")";
    return out.str();
}


FTDI_Device::
~FTDI_Device()
{
    if (_pContext)
        close_interface(this);
}


void
FTDI_Device::
close()
{
    // This is only called by close_interface() when the interface is ready to
    // be closed and released, and while the mutex is already locked.
    if (_pContext)
    {
        DLOG(("") << "closing " << description());
        ftdi_usb_close(_pContext);
        ftdi_free(_pContext);
    }
    _pContext = 0;
}


FTDI_Device::
FTDI_Device(enum ftdi_interface iface, const std::string& product,
            const std::string& manufacturer):
    _pContext(0),
    _iface(iface),
    _manufacturer(manufacturer),
    _product(product),
    _pinDirection(0xFF),
    _mask(0),
    _shift(0)
{
}


bool
FTDI_Device::
open_interface(FTDI_Device* device)
{
    // Look for an existing device with the same context, and if none exists,
    // then let the caller open that context, otherwise just set it directly.
    Synchronized sync(ftdi_device_mutex);

    FTDI_Device* found = nullptr;
    for (auto dp: ftdi_devices)
    {
        if (dp->_iface == device->_iface &&
            dp->_manufacturer == device->_manufacturer &&
            dp->_product == device->_product)
        {
            found = dp;
            break;
        }
    }
    if (!found)
    {
        if (!device->open())
        {
            return false;
        }
    }
    else
    {
        DLOG(("") << "sharing existing context with "
                  << device->description());
        // just assign the existing context
        device->_pContext = found->_pContext;
    }
    ftdi_devices.push_back(device);
    return true;
}


void
FTDI_Device::
close_interface(FTDI_Device* device)
{
    Synchronized sync(ftdi_device_mutex);
    ftdi_context* context = device->_pContext;

    auto it = ftdi_devices.begin();
    auto found = ftdi_devices.end();
    unsigned int ncount = 0;
    for ( ; it != ftdi_devices.end(); ++it)
    {
        if ((*it) == device)
            found = it;
        else if ((*it)->_pContext == context)
            ++ncount;
    }
    if (ncount == 0)
    {
        // no more references, close it.
        device->close();
    }
    else
    {
        DLOG(("") << "unsharing context without closing for "
                  << device->description());
        // reset the context pointer just to be sure it isn't used by this
        // device again.
        device->_pContext = 0;
    }
    // and remove this device from the list.
    ftdi_devices.erase(found);
}


bool
FTDI_Device::
open()
{
    // This is called by open_interface() only when the interface this device
    // needs has not already been created and setup, and it is called while
    // the mutex is locked.
    int status;
    _pContext = ftdi_new();
    if (!_pContext)
    {
        DLOG(("") << error_string("ftdi_new() failed"));
        close();
        return false;
    }

    // The libftdi examples do not call ftdi_init(), so I assume ftdi_new()
    // takes care of it and this was never necessary.

    //
    // bool retval = !ftdi_init(_pContext);
    // if (retval) {
    //     _pContext->module_detach_mode = autoDetach ? AUTO_DETACH_SIO_MODULE : DONT_DETACH_SIO_MODULE;
    //     DLOG(("FtdiGpio<%s, %s>::init(): %s auto-detaching sio module...",
    //           device2Str(DEVICE), iface2Str(IFACE), autoDetach ? "" : "not" ));
    // }

    if ((status = ftdi_set_interface(_pContext, _iface)) != 0)
    {
        DLOG(("") << "ftdi_set_interface(" << _iface << "): "
                  << error_string("", status));
        close();
        return false;
    }

    status = ftdi_usb_open_desc(_pContext, (int)0x0403,
                                (int)0x6011, _product.c_str(), 0);
    if (status == 0)
    {
        DLOG(("") << "opened: " << description());
    }
    else {
        DLOG(("") << error_string("open failed", status));
        close();
        return false;
    }

    if (_product == PRODUCT_GPIO)
    {
        // all pins on the GPIO chip interfaces are outputs
        _pinDirection = 0xFF;
    }
    if (_product == PRODUCT_I2C)
    {
        if (_iface == INTERFACE_C)
        {
            // outgoing GPIO for LED indicators on bits 6 and 7, 
            // incoming GPIO for switch inputs come in on bits 4 & 5
            // outgoing GPIO for power board control on bits 0-3
            _pinDirection = 0xCF;
        }
        if (_iface == INTERFACE_D)
        {
            _pinDirection = 0xFF;
        }
    }
    status = ftdi_set_bitmode(_pContext, _pinDirection, BITMODE_BITBANG);
    if (status == 0)
    {
        DLOG(("") << "set bitbang mode on " << description());
    }
    else
    {
        DLOG(("") << error_string("bitbang mode failed", status));
        close();
        return false;
    }
    return true;
}


void
FTDI_Device::
set_mask(unsigned char mask)
{
    _mask = mask;
    for (_shift = 0; _shift <= 8; ++_shift)
    {
        if ((_mask >> _shift) & 0x01)
            break;
    }
    DLOG(("") << "set_mask " << description()
              << ": mask=0x" << std::hex << int(_mask)
              << ", shift=" << std::dec << _shift);
}


void
FTDI_Device::
write_bits(unsigned char bits)
{
    Synchronized sync(ftdi_device_mutex);
    unsigned char pins = 0;
    int status;
    if (!_read_bits(&pins))
    {
        return;
    }
    // first clear the bits that will be set, then or in the ones we want
    // to set after shifting them to line up with the mask.
    pins &= ~_mask;
    pins |= (bits << _shift);
    DLOG(("") << "(after shift) write_data=0x" << std::hex << int(pins) << ", "
              << description());
    if ((status = ftdi_write_data(_pContext, &pins, 1)) != 1)
    {
        PLOG(("") << error_string("ftdi_write_data", status));
    }
}


bool
FTDI_Device::
read_bits(unsigned char* pins)
{
    Synchronized sync(ftdi_device_mutex);
    auto good = _read_bits(pins);
    if (good)
    {
        // return just the interested bitfield in the rightmost bits.
        *pins &= _mask;
        *pins = *pins >> _shift;
        DLOG(("") << "(after shift) read_bits=0x" << std::hex << int(*pins)
                  << ", " << description());
    }
    return good;
}


bool
FTDI_Device::
_read_bits(unsigned char* pins)
{
    // Note that we call ftdi_read_pins() and not ftdi_read_data() to
    // "circumvent the read buffer, useful for bitbang mode".  There is no
    // corresponding ftdi_write_pins(), only ftdi_write_data().
    int status;
    if ((status = ftdi_read_pins(_pContext, pins)) != 0)
    {
        PLOG(("") << error_string("ftdi_read_pins", status));
        return false;
    }
    DLOG(("") << "read_data=0x" << std::hex << int(*pins) << ", "
                << description());
    return true;
}


std::string
FTDI_Device::
error_string(const std::string& context, int status)
{
    const char* msg = ftdi_get_error_string(_pContext);
    if (!msg && !_pContext)
    {
        msg = "no ftdi context";
    }
    if (!msg)
    {
        msg = "no error";
    }
    std::ostringstream out;
    if (context.size())
        out << context << ": ";
    out << description() << " ";
    out << msg;
    out << " (status " << status << ")";
    return out.str();
}


std::unique_ptr<FTDI_Device>
FTDI_Device::create(const HardwareDevice& device, InterfaceType itype)
{
    // figure out the ftdi device and interface.  to return an actual
    // implementation for this interface and this device, we must be able to
    // find it in the address table.  outputs are easy because every device is
    // an output.
    const ftdi_location* location;
    location = get_ftdi_address(device, itype);
    if (!location)
    {
        const char* itypename = (itype == OUTPUT ? "output" :
                                 (itype == PORT ? "port" : "button"));
        DLOG(("") << "device does not have a ftdi " << itypename
                  << " interface: " << device);
        return nullptr;
    }
    // we could use make_unique here, except the constructor is private.
    std::unique_ptr<FTDI_Device> ftdi;
    ftdi.reset(new FTDI_Device(location->iface, location->product));
    // no harm in setting mask before opening, and it makes the mask settings
    // visible in log messages sooner.
    ftdi->set_mask(location->mask);
    if (!open_interface(ftdi.get()))
    {
        DLOG(("") << "ftdi open failed, "
                  << "no output interface available for device: "
                  << device);
        return nullptr;
    }
    return ftdi;
}


/**
 * OutputInterface implementation for an FTDI GPIO output.
 */
class OutputInterfaceFTDI: public OutputInterface
{
public:
    OutputInterfaceFTDI(std::unique_ptr<FTDI_Device>& ftdi):
        _ftdi(std::move(ftdi))
    {
    }

    OutputState getState() override
    {
        DLOG(("") << "Output::getState()");
        unsigned char bits;
        if (_ftdi->read_bits(&bits))
        {
            return (bits != 0) ? OutputState::ON : OutputState::OFF;
        }
        return OutputState::UNKNOWN;
    }

    void setState(OutputState state) override
    {
        DLOG(("") << "Output::setState(" << state << ")");
        _ftdi->write_bits((state == OutputState::ON) ? 1 : 0);
    }

private:
    std::unique_ptr<FTDI_Device> _ftdi;

};


struct portbits_t {
    PortType ptype;
    unsigned char bits;
};

using portbits_vector_t = std::vector<portbits_t>;

// These are the bit values for the port tansceiver mode, two bits for the
// Exar SP339 transceiver control lines M0 and M1 and one bit for termination.
static const unsigned char LOOPBACK_BITS = 0x00;
static const unsigned char RS232_BITS = 0x01;
static const unsigned char RS485_HALF_BITS = 0x02;
static const unsigned char RS422_RS485_BITS = 0x03;
static const unsigned char TERM_120_OHM_BIT = 0x04;

static portbits_vector_t PORTBITS {
    { PortType::RS422, RS422_RS485_BITS },
    { PortType::RS485_HALF, RS485_HALF_BITS },
    { PortType::RS232, RS232_BITS },
    { PortType::LOOPBACK, LOOPBACK_BITS }
};

unsigned char
ptype_to_bits(PortType ptype)
{
    for (auto& pbits: PORTBITS)
    {
        if (pbits.ptype == ptype)
            return pbits.bits;
    }
    return 0;
}

PortType
bits_to_ptype(unsigned char bits)
{
    bits = bits & 0x03;
    for (auto& pbits: PORTBITS)
    {
        if (pbits.bits == bits)
            return pbits.ptype;
    }
    return PortType::LOOPBACK;
}


class SerialPortInterfaceFTDI : public SerialPortInterface
{
public:
    SerialPortInterfaceFTDI(std::unique_ptr<FTDI_Device>& ftdi):
        _ftdi(std::move(ftdi))
    {}

    void
    getConfig(PortType& ptype, PortTermination& term)
    {
        DLOG(("") << "getConfig()");
        unsigned char bits{0};
        if (_ftdi->read_bits(&bits))
        {
            ptype = bits_to_ptype(bits);
            term = (bits & TERM_120_OHM_BIT) ? PortTermination::TERM_ON :
                                               PortTermination::NO_TERM;
        }
    }

    void
    setConfig(PortType ptype, PortTermination term)
    {
        DLOG(("") << "setConfig(" << ptype << "," << term << ")");
        unsigned char bits{0};
        bits |= ptype_to_bits(ptype);
        bits |= (term == PortTermination::TERM_ON) ? TERM_120_OHM_BIT : 0;
        _ftdi->write_bits(bits);
    }

private:
    std::unique_ptr<FTDI_Device> _ftdi;
};


class ButtonInterfaceFTDI : public ButtonInterface
{
public:

    ButtonInterfaceFTDI(std::unique_ptr<FTDI_Device>& ftdi):
        _ftdi(std::move(ftdi))
    {}

    STATE getState() override
    {
        unsigned char bits;
        if (_ftdi->read_bits(&bits))
        {
            return (bits != 0) ? DOWN : UP;
        }
        return UNKNOWN;
    }

private:
    std::unique_ptr<FTDI_Device> _ftdi;
};



/**
 * HardwareInterface implementation for the FTDI devices on DSM3.
 *
 * The USB interfaces on the ftdi chips can only be claimed once, so the
 * hardware interface cannot keep the devices open for the duration of the
 * program.  Technically it would be better if there were a single daemon
 * (perhaps a systemd unit which started on access to a unix port or dbus
 * endpoint) which could claim the interfaces and synchronize access from
 * multiple processes.  However, short of that, and perhaps even in that case,
 * the devices are kept open only long enough to handle calls on the
 * interface.  That's the point of using a scoped shared pointer to access the
 * hardware interface implementation.  The interfaces for all the hardware
 * devices are implemented using the FTDI_Device adapter.  The adapter takes
 * care of sharing contexts across devices which share a GPIO 8-bit interface
 * on a FTDI chip, and it takes share of letting the specific interface
 * manipulate only it's specific bit field on that interface.
 *
 * The device interfaces are cached in the HardwareInterface base class, and
 * each interface keeps a smart pointer to it's FTDI_Device instance.  When
 * the implementation goes out of scope and is destroyed, all the devices are
 * destroyed and all the ftdi contexts are closed.
 */
class HardwareInterfaceFTDI: public HardwareInterface
{
public:
    HardwareInterfaceFTDI();

    virtual ~HardwareInterfaceFTDI();

protected:

    SerialPortInterface*
    createSerialPortInterface(HardwareDeviceImpl* dimpl) override;

    OutputInterface*
    createOutputInterface(HardwareDeviceImpl* dimpl) override;

    ButtonInterface*
    createButtonInterface(HardwareDeviceImpl* dimpl) override;

};


HardwareInterface*
get_hardware_interface_ftdi()
{
    return new HardwareInterfaceFTDI();
}


HardwareInterfaceFTDI::
HardwareInterfaceFTDI():
    HardwareInterface("ftdi")
{}


SerialPortInterface*
HardwareInterfaceFTDI::
createSerialPortInterface(HardwareDeviceImpl* dimpl)
{
    // Create the interface implementation with the FTDI_Device.  The returned
    // pointer will be cached by the base class.
    auto ftdi = FTDI_Device::create(HardwareDevice(dimpl->_id), PORT);
    if (ftdi)
        return new SerialPortInterfaceFTDI(ftdi);
    return nullptr;
}

OutputInterface*
HardwareInterfaceFTDI::
createOutputInterface(HardwareDeviceImpl* dimpl)
{
    // Create the interface implementation with the FTDI_Device.  The returned
    // pointer will be cached by the base class.
    auto ftdi = FTDI_Device::create(HardwareDevice(dimpl->_id), OUTPUT);
    if (ftdi)
        return new OutputInterfaceFTDI(ftdi);
    return nullptr;
}

ButtonInterface*
HardwareInterfaceFTDI::
createButtonInterface(HardwareDeviceImpl* dimpl)
{
    // Create the interface implementation with the FTDI_Device.  The returned
    // pointer will be cached by the base class.
    auto ftdi = FTDI_Device::create(HardwareDevice(dimpl->_id), BUTTON);
    if (ftdi)
        return new ButtonInterfaceFTDI(ftdi);
    return nullptr;
}


HardwareInterfaceFTDI::
~HardwareInterfaceFTDI()
{
    // All the interface implementations should be deleted when the base class
    // cache is destroyed.  However, those implementations depend on the mutex
    // defined in this module, and that might be destroyed before the
    // implementations.  That shouldn't happen as long as the implementation
    // is not open when the program exits, but it doesn't hurt to leave it
    // here.
    DLOG(("") << "deleting HardwareInterfaceFTDI and devices...");
    delete_devices();
}


} // namespace core
} // namespace nidas
