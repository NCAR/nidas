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
#include "nidas/util/Logger.h"

#include <ftdi.h>
#include <map>
#include <sstream>

namespace {

using namespace nidas::core;

// deduces the FT4232H GPIO interface form the port in _xcvrConfig.
// need to select the interface based on the specified port
// at present, assume 4 bits per port definition
enum ftdi_interface port2iface(const HardwareDevice& device)
{
    static std::map<std::string, ftdi_interface> interfaces{
        { Devices::PORT0.id(), INTERFACE_A },
        { Devices::PORT1.id(), INTERFACE_A },
        { Devices::PORT2.id(), INTERFACE_B },
        { Devices::PORT3.id(), INTERFACE_B },
        { Devices::PORT4.id(), INTERFACE_C },
        { Devices::PORT5.id(), INTERFACE_C },
        { Devices::PORT6.id(), INTERFACE_D },
        { Devices::PORT7.id(), INTERFACE_D }
    };
    enum ftdi_interface iface = INTERFACE_ANY;

    auto it = interfaces.find(device.id());
    if (it != interfaces.end())
        iface = it->second;

    return iface;
}


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


const unsigned char XCVR_BITS_PORT_TYPE = 0b00000011;
const unsigned char XCVR_BITS_TERM =      0b00000100;
const unsigned char SENSOR_BITS_POWER =   0b00001000;


const std::string MANUFACTURER_UCAR{"UCAR"};

const std::string PRODUCT_GPIO{"GPIO"};
const std::string PRODUCT_I2C{"I2C"};
const std::string PRODUCT_P0_P3{"P0-P3"};
const std::string PRODUCT_P4_P7{"P4-P7"};

}



namespace nidas {
namespace core {

/**
 * @brief HardwareInterface implementation for the FTDI devices on DSM3.
 *
 * 
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
class DSM_FTDI
{
public:
    DSM_FTDI(enum ftdi_interface iface, const std::string& product,
             const std::string& manufacturer = MANUFACTURER_UCAR);

    std::string
    description();

    std::string
    error_string();

    void
    close();

    ~DSM_FTDI();

    DSM_FTDI(const DSM_FTDI&) = delete;
    DSM_FTDI&
    operator=(const DSM_FTDI&) = delete;

private:
    ftdi_context* _pContext;
    ftdi_interface _iface;
    std::string _manufacturer;
    std::string _product;
    int _pinDirection;
};


std::string
DSM_FTDI::
description()
{
    std::ostringstream out;
    out << "ftdi(" << _product + "," << ifaceToString(_iface);
    out << ",pins=" << std::hex << _pinDirection << ")";
    return out.str();
}


DSM_FTDI::
~DSM_FTDI()
{
    close();
}


void
DSM_FTDI::
close()
{
    if (_pContext)
    {
        ftdi_usb_close(_pContext);
        ftdi_free(_pContext);
    }
    _pContext = 0;
}


DSM_FTDI::
DSM_FTDI(enum ftdi_interface iface, const std::string& product,
            const std::string& manufacturer):
    _pContext(0),
    _iface(iface),
    _manufacturer(manufacturer),
    _product(product),
    _pinDirection(0xFF)
{
    _pContext = ftdi_new();
    if (!_pContext)
    {
        DLOG(("") << "ftdi_new() failed: " << error_string());
        close();
        return;
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

    if (ftdi_set_interface(_pContext, iface))
    {
        close();
        return;
    }

    int status = ftdi_usb_open_desc(_pContext, (int)0x0403,
                                    (int)0x6011, _product.c_str(), 0);
    if (status == 0)
    {
        DLOG(("") << "opened: " << description());
    }
    else {
        DLOG(("") << "open failed on " << description() << ": "
                  << error_string() << " - status: " << status);
        close();
        return;
    }

    if (_product == PRODUCT_I2C)
    {
        if (iface == INTERFACE_C)
        {
            // outgoing GPIO for LED indicators on bits 6 and 7, 
            // incoming GPIO for switch inputs come in on bits 4 & 5
            // outgoing GPIO for power board control on bits 0-3
            _pinDirection = 0xCF;
        }
        if (iface == INTERFACE_D)
        {
            _pinDirection = 0xFF;
        }
        status = ftdi_set_bitmode(_pContext, _pinDirection, BITMODE_BITBANG);
        if (status == 0)
        {
            DLOG(("") << "set bitbang mode on " << description());
        }
        else
        {
            DLOG(("") << "bitbang mode failed on " << description() << ": "
                      << error_string() << " - status: " << status);
            close();
            return;
        }
    }
}


std::string
DSM_FTDI::
error_string()
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
    return msg;
}



/**
 * @brief An FTDI output has a device
 * 
 */
class OutputInterfaceFTDI: OutputInterface
{
public:


};




class HardwareInterfaceFTDI: public HardwareInterface
{
public:
    HardwareInterfaceFTDI();

    virtual ~HardwareInterfaceFTDI();

    virtual SerialPortInterface*
    getSerialPortInterface(const HardwareDevice& device);

    virtual OutputInterface*
    getOutputInterface(const HardwareDevice& device);

    virtual ButtonInterface*
    getButtonInterface(const HardwareDevice& device);

private:


};


HardwareInterfaceFTDI::
HardwareInterfaceFTDI():
    HardwareInterface("ftdi")
{}


SerialPortInterface*
HardwareInterfaceFTDI::
getSerialPortInterface(const HardwareDevice& device)
{
    return nullptr;
}


ButtonInterface*
HardwareInterfaceFTDI::
getButtonInterface(const HardwareDevice& device)
{
    return nullptr;
}


OutputInterface*
HardwareInterfaceFTDI::
getOutputInterface(const HardwareDevice& device)
{
    // figure out the 

    return nullptr;
}


HardwareInterfaceFTDI::
~HardwareInterfaceFTDI()
{}



} // namespace core
} // namespace nidas
