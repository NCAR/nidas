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

#include <ftdi.h>
#include <map>
#include <memory>
#include <sstream>

namespace {

using namespace nidas::core;

std::string
ifaceToString(ftdi_interface iface)
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
    const std::string product;
    enum ftdi_interface iface;
    unsigned char mask;
};


/**
 * Every serial port sensor gets half of an ftdi interface on the GPIO chip.
 * Port 0 is the low 4 bits of interface A, and port 1 is the high 4 bits, and
 * so on up to port 7 as the high 4 bits of interface D.  These masks are for
 * the 4 bits after they have been shifted to low nybble.
 */
const unsigned char XCVR_BITS_PORT_TYPE = 0b00000011;
const unsigned char XCVR_BITS_TERM =      0b00000100;
const unsigned char SENSOR_BITS_POWER =   0b00001000;

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

using namespace nidas::core::Devices;

/**
 * Return the information needed to address the specific bits for the given
 * device.  The mask should be and'ed with the bits above to get the
 * particular bit to set (output) or read (input).  Return nullptr the device
 * is not recognized.
 */
const ftdi_location*
get_ftdi_address(const HardwareDevice& device)
{
    static std::vector<ftdi_location> locations
    {
        { PORT0, PRODUCT_GPIO, INTERFACE_A, 0b00001111 },
        { PORT1, PRODUCT_GPIO, INTERFACE_A, 0b11110000 },
        { PORT2, PRODUCT_GPIO, INTERFACE_A, 0b00001111 },
        { PORT3, PRODUCT_GPIO, INTERFACE_A, 0b11110000 },
        { PORT4, PRODUCT_GPIO, INTERFACE_A, 0b00001111 },
        { PORT5, PRODUCT_GPIO, INTERFACE_A, 0b11110000 },
        { PORT6, PRODUCT_GPIO, INTERFACE_A, 0b00001111 },
        { PORT7, PRODUCT_GPIO, INTERFACE_A, 0b11110000 },
        { DCDC, PRODUCT_I2C, INTERFACE_C, PWR_BITS_DCDC },
        { AUX, PRODUCT_I2C, INTERFACE_C, PWR_BITS_AUX },
        { BANK1, PRODUCT_I2C, INTERFACE_C, PWR_BITS_BANK1 },
        { BANK2, PRODUCT_I2C, INTERFACE_C, PWR_BITS_BANK2 },
        { WIFI, PRODUCT_I2C, INTERFACE_C,
          WIFI_LED_BITS | WIFI_BUTTON_BITS },
        { P1, PRODUCT_I2C, INTERFACE_C, P1_LED_BITS | P1_BUTTON_BITS }
    };
    for (auto& lp : locations)
    {
        if (lp.device.id() == device.id())
            return &lp;
    }
    return nullptr;
}


} // namespace anonymous


namespace nidas {
namespace core {

/**
 * HardwareInterface implementation for the FTDI devices on DSM3.
 */


/**
 * @brief Adapter for the libftdi interface.
 *
 * This associates a specific DSM device and interface with a libftdi context.
 * It is based on the original FtdiGpio<DEVICE, IFACE> template
 * implementation, but it is simplified by not using templates and not using
 * shared or weak pointers.
 *
 */
class FTDI_Device
{
public:

    /**
     * Create and open the FTDI_Device instance for the given device, or else
     * return null if it fails.
     */
    static FTDI_Device*
    create(const HardwareDevice& device);

    /**
     * Setup but do not open() a FTDI_Device for @p iface on the USB device with
     * the given @p manufacturer and @p product.  The specific bits to be
     * managed on the interface must be set with set_mask().
     */
    FTDI_Device(enum ftdi_interface iface, const std::string& product,
             const std::string& manufacturer=MANUFACTURER_UCAR);

    /**
     * Open the FTDI device, returning false if the open fails.
     */
    bool
    open();

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

    /**
     * Format an ftdi error message, including the given context, the
     * description of this FTDI_DSM object, the error string from the ftdi
     * library, and the status return code.
     *
     * @param context 
     */
    std::string
    error_string(const std::string& context="", int status=0);

    void
    close();

    ~FTDI_Device();

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


std::string
FTDI_Device::
description()
{
    std::ostringstream out;
    out << "ftdi(" << _product + "," << ifaceToString(_iface);
    out << ",pins=" << std::hex << (int)_pinDirection
        << ",mask=" << std::hex << (int)_mask
        << ",shift=" << std::dec << _shift << ")";
    return out.str();
}


FTDI_Device::
~FTDI_Device()
{
    close();
}


void
FTDI_Device::
close()
{
    if (_pContext)
    {
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
open()
{
    close();
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
    unsigned int i;
    for (i = 0; (i < 8) && ((_mask >> i) ^ 0b1); ++i);
    _shift = i;
    VLOG(("") << "set_mask " << description()
              << ": mask=" << std::hex << _mask
              << ", shift=" << std::dec << _shift);
}


void
FTDI_Device::
write_bits(unsigned char bits)
{
    unsigned char pins = 0;
    int status;
    if (!read_bits(&pins))
    {
        return;
    }
    // first clear the bits that will be set, then or in the ones we want
    // to set after shifting them to line up with the mask.
    pins &= ~_mask;
    pins |= (bits << _shift);
    if ((status = ftdi_write_data(_pContext, &pins, 1)) != 1)
    {
        PLOG(("") << error_string("ftdi_write_data", status));
    }
}


bool
FTDI_Device::
read_bits(unsigned char* pins)
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


FTDI_Device*
FTDI_Device::create(const HardwareDevice& device)
{
    // figure out the ftdi device and interface.  to return an actual
    // implementation for this interface and this device, we must be able to
    // find it in the address table.  outputs are easy because every device is
    // an output.
    const ftdi_location* location;
    location = get_ftdi_address(device);
    if (!location)
    {
        DLOG(("") << "device does not have an interface for ftdi:"
                  << device.id());
        return nullptr;
    }
    std::unique_ptr<FTDI_Device> ftdi(new FTDI_Device(location->iface, location->product));
    // no harm in setting mask before opening, and it makes the mask settings
    // visible in log messages sooner.
    ftdi->set_mask(location->mask);
    if (!ftdi->open())
    {
        DLOG(("") << "ftdi open failed, "
                  << "no output interface available for device: "
                  << device.id());
        return nullptr;
    }
    return ftdi.release();
}


/**
 * OutputInterface implementation for an FTDI GPIO output.
 */
class OutputInterfaceFTDI: public OutputInterface
{
public:
    OutputInterfaceFTDI(FTDI_Device* ftdi):
        _ftdi(ftdi)
    {
    }

    STATE getState() override
    {
        unsigned char bits;
        if (_ftdi->read_bits(&bits))
        {
            return (bits != 0) ? ON : OFF;
        }
        return UNKNOWN;
    }

    void setState(STATE state) override
    {
        _ftdi->write_bits((state == ON) ? 1 : 0);
    }

private:
    std::unique_ptr<FTDI_Device> _ftdi;

};


using PORT_TYPES = SerialPortInterface::PORT_TYPES;

struct portbits_t {
    PORT_TYPES ptype;
    unsigned char bits;
};

using portbits_vector_t = std::vector<portbits_t>;

static const unsigned char LOOPBACK_BITS = 0x00;
static const unsigned char RS232_BITS = 0x01;
static const unsigned char RS485_HALF_BITS = 0x02;
static const unsigned char RS422_RS485_BITS = 0x03;
static const unsigned char TERM_120_OHM_BIT = 0x04;
static const unsigned char SENSOR_POWER_ON_BIT = 0x08;

static portbits_vector_t PORTBITS {
    { SerialPortInterface::RS422, RS422_RS485_BITS },
    { SerialPortInterface::RS485_HALF, RS485_HALF_BITS },
    { SerialPortInterface::RS232, RS232_BITS },
    { SerialPortInterface::LOOPBACK, LOOPBACK_BITS }
};

unsigned char
ptype_to_bits(PORT_TYPES ptype)
{
    for (auto& pbits: PORTBITS)
    {
        if (pbits.ptype == ptype)
            return pbits.bits;
    }
    return 0;
}

PORT_TYPES
bits_to_ptype(unsigned char bits)
{
    bits = bits & 0x03;
    for (auto& pbits: PORTBITS)
    {
        if (pbits.bits == bits)
            return pbits.ptype;
    }
    return SerialPortInterface::LOOPBACK;
}



class SerialPortInterfaceFTDI : public SerialPortInterface
{
public:
    SerialPortInterfaceFTDI(FTDI_Device* ftdi):
        _ftdi(ftdi)
    {}

    void
    getConfig(PORT_TYPES* ptype, TERM* term)
    {
        unsigned char bits;
        if (_ftdi->read_bits(&bits))
        {
            bits >>= _ftdi->_shift;
            if (ptype)
                *ptype = bits_to_ptype(bits);
            if (term)
                *term = (bits & TERM_120_OHM_BIT) ? TERM_120_OHM : NO_TERM;
        }
    }

    void
    setConfig(PORT_TYPES* ptype, TERM* term)
    {
        unsigned char bits;
        if (_ftdi->read_bits(&bits))
        {
            bits >>= _ftdi->_shift;
            if (ptype)
            {
                bits &= ~0x03;
                bits |= ptype_to_bits(*ptype);
            }
            if (term)
            {
                bits &= ~TERM_120_OHM_BIT;
                bits |= (*term == TERM_120_OHM) ? TERM_120_OHM_BIT : 0;
            }
            bits <<= _ftdi->_shift;
            _ftdi->write_bits(bits);
        }
    }

private:
    std::unique_ptr<FTDI_Device> _ftdi;
};


class ButtonInterfaceFTDI : public ButtonInterface
{
public:

    ButtonInterfaceFTDI(FTDI_Device* ftdi):
        _ftdi(ftdi)
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
    if (auto ftdi = FTDI_Device::create(HardwareDevice(dimpl->_id)))
        return new SerialPortInterfaceFTDI(ftdi);
    return nullptr;
}

OutputInterface*
HardwareInterfaceFTDI::
createOutputInterface(HardwareDeviceImpl* dimpl)
{
    // Create the interface implementation with the FTDI_Device.  The returned
    // pointer will be cached by the base class.
    if (auto ftdi = FTDI_Device::create(HardwareDevice(dimpl->_id)))
        return new OutputInterfaceFTDI(ftdi);
    return nullptr;
}

ButtonInterface*
HardwareInterfaceFTDI::
createButtonInterface(HardwareDeviceImpl* dimpl)
{
    // Create the interface implementation with the FTDI_Device.  The returned
    // pointer will be cached by the base class.
    if (auto ftdi = FTDI_Device::create(HardwareDevice(dimpl->_id)))
        return new ButtonInterfaceFTDI(ftdi);
    return nullptr;
}



HardwareInterfaceFTDI::
~HardwareInterfaceFTDI()
{
    // XXXX make sure all the interface implementations get deleted.
}



} // namespace core
} // namespace nidas
