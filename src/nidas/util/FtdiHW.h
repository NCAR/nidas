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
#ifndef NIDAS_UTIL_FTDIHW_H
#define NIDAS_UTIL_FTDIHW_H

#include <string>
#include <libusb-1.0/libusb.h>
#include <ftdi.h>

#include "Logger.h"

#include "ThreadSupport.h"
#include "IOException.h"
#include "Logger.h"
#include "InvalidParameterException.h"
#include "GpioIF.h"

struct libusb_device;

namespace nidas { namespace util {

enum FTDI_DEVICES {FTDI_ILLEGAL_DEVICE=-1, FTDI_I2C, FTDI_GPIO, FTDI_P0_P3, FTDI_P4_P7, NUM_FTDI_DEVICES};
inline const char* device2Str(FTDI_DEVICES device)
{
    std::string retval;

    switch (device) {
        case FTDI_I2C:
            retval = "FTDI_I2C";
            break;
        case FTDI_GPIO:
            retval = "FTDI_GPIO";
            break;
        case FTDI_P0_P3:
            retval = "FTDI_P0_P3";
            break;
        case FTDI_P4_P7:
            retval = "FTDI_P4_P7";
            break;
        case FTDI_ILLEGAL_DEVICE:
        default:
            retval = "ILLEGAL_DEVICE";
            break;
    }

    return retval.c_str();
}

inline const char* iface2Str(ftdi_interface iface)
{
    std::string retval = "";

    switch (iface) {
        case INTERFACE_A:
            retval = "IFACE_A";
            break;
        case INTERFACE_B:
            retval = "IFACE_B";
            break;
        case INTERFACE_C:
            retval = "IFACE_C";
            break;
        case INTERFACE_D:
            retval = "IFACE_D";
            break;
        case INTERFACE_ANY:
        default:
            retval = "IFACE_ANY";
            break;
    }

    return retval.c_str();
}

/*
 *  FtdiHwIF interface class
 */
class FtdiHwIF : public GpioIF
{
public:
    virtual ~FtdiHwIF() {}

    /*
     *  Used to set the operational mode of the device, usually bitbang mode.
     */
    virtual bool setMode(unsigned char mask, unsigned char mode) = 0;

    /*
     *  Used to set the device interface, upon which the operations in this class operate.
     */
    virtual bool setInterface(ftdi_interface iface) = 0;

    virtual ftdi_interface getInterface() = 0;

    // safe FT4232H open - must bracket all bitbang type operations!!!
    // returns w/o action, if already open.
    virtual void open() = 0;

    // safe FT4232H close - must bracket all FTDI bitbang type operations!!!
    // returns w/o action, if already closed.
    virtual void close() = 0;

    /*
     *  Helper method to return the open status of the device.
     */
    virtual bool isOpen() = 0;

    /*
     * returns the last textual status of the last operation.
     */
    virtual std::string error_string() = 0;

};


/*
 *  Generic FTDI device aimed at a particular interface on a device with an eeprom which has been programmed
 *  with specified manufacturer and product description strings. It is further specialized by specifying which
 *  FTDI interface/port is to be used for the operations performed by the methods.
 *
 *  NOTE: Class FtdiGpio<DEVICE, IFACE> is *not* intended to be used on its own, and so the constructor is not public.
 *        This is due to the fact that a single interface (i.e. byte-wide I/O port) can be assigned multiple
 *        responsibilities in different domains. For instance, one domain might be serial transceiver control
 *        while another domain might be scientific instrument power control. This is a very real use case in
 *        NIDAS for those systems which use the new EOL serial port boards.
 */
template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
class FtdiGpio : public FtdiHwIF
{
public:
    // static singleton getter
    static FtdiGpio<DEVICE, IFACE>* getFtdiGpio(const std::string manufStr = "UCAR", const std::string productStr = "GPIO");

    // static singleton destroyer
    static void deleteFtdiGpio();

    /*
     *  Helper method to return the device found status
     */
    virtual bool ifaceFound();

    // safe FT4232H open - must bracket all bitbang type operations!!!
    // returns w/o action, if already open.
    virtual void open();

    // safe FT4232H close - must bracket all FTDI bitbang type operations!!!
    // returns w/o action, if already closed.
    virtual void close();

    /*
     *  Used to set the operational mode of the device, usually bitbang mode.
     */
    virtual bool setMode(unsigned char mask, unsigned char mode);

    /*
     *  Used to set the device interface, upon which the operations in this class operate.
     */
    virtual bool setInterface(ftdi_interface iface =  IFACE);

    virtual ftdi_interface getInterface();

    /*
     *  Helper method to return the open status of the device.
     */
    virtual bool isOpen();

    /*
     * returns the last textual status of the last operation.
     */
    virtual std::string error_string();

    /*
     *  Implements GpioIF::read() pure virtual method
     */
    virtual unsigned char read();

    /*
     *  Implements GpioIF::write() pure virtual method
     */
    virtual void write(unsigned char pins);

private:
    static FtdiGpio<DEVICE, IFACE>* _pFtdiDevice;
    ftdi_context* _pContext;
    std::string _manufStr;
    std::string _productStr;
    bool _foundIface;

    /*
     *  FtdiGpio<DEVICE, IFACE> constructor/destructor.
     *
     *  Make private so that class can be a singleton.
     */
    FtdiGpio(const std::string vendor, const std::string product);
    virtual ~FtdiGpio();

    /*
     *  No copying
     */
    FtdiGpio(const FtdiGpio& rRight);
    FtdiGpio& operator=(const FtdiGpio& rRight);
    FtdiGpio& operator=(FtdiGpio& rRight);
};

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiGpio<DEVICE, IFACE>* FtdiGpio<DEVICE, IFACE>::getFtdiGpio(const std::string manufStr, const std::string productStr)
{
    if (!_pFtdiDevice) {
        DLOG(("Constructing FtdiGpio<%s, %s> singleton",
              device2Str(DEVICE), iface2Str(IFACE)));
        _pFtdiDevice = new FtdiGpio<DEVICE, IFACE>(manufStr, productStr);
    }

    return _pFtdiDevice;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiGpio<DEVICE, IFACE>::deleteFtdiGpio()
{
    delete _pFtdiDevice;
}



template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiGpio<DEVICE, IFACE>::FtdiGpio(const std::string manufStr, const std::string productStr)
: _pContext(ftdi_new()), _manufStr(manufStr), _productStr(productStr), _foundIface(false)
{
    if (!_pContext) {
        throw IOException("FtdiGpio<DEVICE, IFACE>::FtdiGpio()", ": Failed to allocate ftdi_context object.");
    }

    DLOG(("FtdiGpio<%s, %s>(): set interface...",
          device2Str(DEVICE), iface2Str(IFACE)));
    if (setInterface(IFACE)) {
        DLOG(("FtdiGpio<%s, %s>(): successfully set the interface: ",
              device2Str(DEVICE), iface2Str(IFACE)));

        open();
        if (isOpen()) {
            DLOG(("FtdiGpio<%s, %s>(): set bitbang mode",
                  device2Str(DEVICE), iface2Str(IFACE)));
            if (setMode(0xFF, BITMODE_BITBANG)) {
                DLOG(("FtdiGpio<%s, %s>(): Successfully set mode to bitbang",
                      device2Str(DEVICE), iface2Str(IFACE)));
            }
            else {
                DLOG(("FtdiGpio<%s, %s>(): failed to set mode to bitbang: ",
                      device2Str(DEVICE), iface2Str(IFACE)) << error_string());
            }
            close();
        }
        else {
            DLOG(("FtdiGpio<%s, %s>(): failed to open the FTDI device.",
                  device2Str(DEVICE), iface2Str(IFACE)));
        }
    }
    else {
        DLOG(("FtdiGpio<%s, %s>(): Failed to set the interface: ",
              device2Str(DEVICE), iface2Str(IFACE)) << error_string());
    }
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiGpio<DEVICE, IFACE>::~FtdiGpio()
{
    DLOG(("Destroying FtdiGpio<%s, %s>...", device2Str(DEVICE), iface2Str(IFACE)));
//    close();
    ftdi_usb_close(_pContext);
    ftdi_free(_pContext);
    _pContext = 0;
}


template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiGpio<DEVICE, IFACE>::open()
{
    if (!isOpen()) {
        VLOG(("FtdiGpio<%s, %s>::open(): Attempting to open FTDI device...",
              device2Str(DEVICE), iface2Str(IFACE)) << "product: " << _productStr);
        int openStatus = ftdi_usb_open_desc(_pContext, (int)0x0403, (int)0x6011, _productStr.c_str(), 0);
        _foundIface = !openStatus;
        if (ifaceFound()) {
            // TODO: let's check the manuf string as well...

            VLOG(("FtdiGpio<%s, %s>::open(): Successfully opened FTDI device...",
                  device2Str(DEVICE), iface2Str(IFACE)));
        }
        else {
            VLOG(("FtdiGpio<%s, %s>::open(): Failed to open FTDI device: ",
                  device2Str(DEVICE), iface2Str(IFACE)) << error_string() << " - status: " << openStatus);
        }
    }
    else {
        VLOG(("FtdiGpio<%s, %s>::open(): FTDI device already open...",
              device2Str(DEVICE), iface2Str(IFACE)));
    }
}

// safe FTDI close - must bracket all FTDI bitbang type operations!!!
// returns true if already closed or successfully closed, false if attempted close fails.
template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiGpio<DEVICE, IFACE>::close()
{
    if (ifaceFound()) {
        if (isOpen()) {
            VLOG(("FtdiGpio<%s, %s>::close(): Attempting to close FTDI device...",
                  device2Str(DEVICE), iface2Str(IFACE)));
            if (!ftdi_usb_close(_pContext)) {
                VLOG(("FtdiGpio<%s, %s>::close(): Successfully closed FTDI device...",
                      device2Str(DEVICE), iface2Str(IFACE)));
            }
            else {
                VLOG(("FtdiGpio<%s, %s>::close(): Failed to close FTDI device...",
                      device2Str(DEVICE), iface2Str(IFACE)) << error_string());
            }
        }
        else {
            VLOG(("FtdiGpio<%s, %s>::close(): FTDI device already closed...",
                  device2Str(DEVICE), iface2Str(IFACE)));
        }
    }
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
unsigned char FtdiGpio<DEVICE, IFACE>::read()
{
    unsigned char pins = 0;
    if (ifaceFound()) {
        open();
        if (isOpen()) {
            if (!ftdi_read_pins(_pContext, &pins)) {
                DLOG(("FtdiGpio<%s, %s>::read(): Successfully read the device pins: 0x%02x",
                      device2Str(DEVICE), iface2Str(IFACE), pins));
            }
            else {
                DLOG(("FtdiGpio<%s, %s>::read(): Failed to read the device pins...",
                      device2Str(DEVICE), iface2Str(IFACE)) << error_string());
                throw IOException(std::string("FtdiGpio<%s, %s>::read(): Could not read from interface: "),
                                  error_string());
            }
            close();
        }
        else {
            DLOG(("FtdiGpio<%s, %s>::read(): Failed to open FTDI device: ",
                  device2Str(DEVICE), iface2Str(IFACE)) << error_string());
            throw IOException(std::string("FtdiGpio<DEVICE, IFACE>::read(): Failed to open the device"),
                              error_string());
        }
    }

    return pins;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiGpio<DEVICE, IFACE>::write(unsigned char pins)
{
    if (ifaceFound()) {
        open();
        if (ftdi_write_data(_pContext, &pins, 1) < 0) {
            throw IOException(std::string("FtdiGpio<DEVICE, IFACE>::write(): Could not write to interface: "),
                                          error_string());
        }
        close();
    }
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiGpio<DEVICE, IFACE>::ifaceFound()
{
    return _foundIface;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiGpio<DEVICE, IFACE>::setMode(unsigned char mask, unsigned char mode) {
    bool retval = true;
    if (ifaceFound()) {
        retval = !ftdi_set_bitmode(_pContext, mask, mode);
    }

    return retval;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiGpio<DEVICE, IFACE>::setInterface(ftdi_interface iface) {
    return !ftdi_set_interface(_pContext, iface);
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
ftdi_interface FtdiGpio<DEVICE, IFACE>::getInterface()
{
    return static_cast<ftdi_interface>(_pContext->interface);
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiGpio<DEVICE, IFACE>::isOpen()
{
    return  _pContext->usb_dev != 0;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
std::string FtdiGpio<DEVICE, IFACE>::error_string()
{
    std::string retval;
    if (ftdi_get_error_string(_pContext)) {
        retval.append(ftdi_get_error_string(_pContext));
    }

    return retval;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiGpio<DEVICE, IFACE>* FtdiGpio<DEVICE, IFACE>::_pFtdiDevice = 0;

/*
 *  Helper method to get the right FtdiGpio<DEVICE, IFACE> singleton
 */
inline FtdiHwIF* getFtdiDevice(FTDI_DEVICES device, ftdi_interface iface) {
    FtdiHwIF* pFtdiDevice = 0;

    switch (device) {
    case FTDI_I2C:
		switch (iface) {
		case INTERFACE_A:
			DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_I2C, INTERFACE_A> singleton..."));
			pFtdiDevice = FtdiGpio<FTDI_I2C, INTERFACE_A>::getFtdiGpio("UCAR", "I2C");
			break;
		case INTERFACE_B:
			DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_I2C, INTERFACE_B> singleton..."));
			pFtdiDevice = FtdiGpio<FTDI_I2C, INTERFACE_B>::getFtdiGpio("UCAR", "I2C");
			break;
		case INTERFACE_C:
			DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_I2C, INTERFACE_C> singleton..."));
			pFtdiDevice = FtdiGpio<FTDI_I2C, INTERFACE_C>::getFtdiGpio("UCAR", "I2C");
			break;
		case INTERFACE_D:
			DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_I2C, INTERFACE_D> singleton..."));
			pFtdiDevice = FtdiGpio<FTDI_I2C, INTERFACE_D>::getFtdiGpio("UCAR", "I2C");
			break;
		default:
			ILOG(("getFtdiDevice(): Illegal ftdi_interface value for device, I2C!!"));
			throw InvalidParameterException("FtdiHW", "getFtdiDevice()", "Illegal ftdi_interface value!!");
			break;
		}
		break;

	    case FTDI_GPIO:
			switch (iface) {
			case INTERFACE_A:
				DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_GPIO, INTERFACE_A> singleton..."));
				pFtdiDevice = FtdiGpio<FTDI_GPIO, INTERFACE_A>::getFtdiGpio("UCAR", "GPIO");
				break;
			case INTERFACE_B:
				DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_GPIO, INTERFACE_B> singleton..."));
				pFtdiDevice = FtdiGpio<FTDI_GPIO, INTERFACE_B>::getFtdiGpio("UCAR", "GPIO");
				break;
			case INTERFACE_C:
				DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_GPIO, INTERFACE_C> singleton..."));
				pFtdiDevice = FtdiGpio<FTDI_GPIO, INTERFACE_C>::getFtdiGpio("UCAR", "GPIO");
				break;
			case INTERFACE_D:
				DLOG(("getFtdiDevice(): getting FtdiGpio<FTDI_GPIO, INTERFACE_D> singleton..."));
				pFtdiDevice = FtdiGpio<FTDI_GPIO, INTERFACE_D>::getFtdiGpio("UCAR", "GPIO");
				break;
			default:
				ILOG(("getFtdiDevice(): Illegal ftdi_interface value for GPIO device!!"));
				throw InvalidParameterException("FtdiHW", "getFtdiDevice()", "Illegal ftdi_interface value!!");
				break;
			}
			break;

		default:
			ILOG(("getFtdiDevice(): Illegal FTDI_DEVICES value!!"));
			throw InvalidParameterException("FtdiHW", "getFtdiDevice()", "Illegal FTDI_DEVICES value!!");
			break;
    }
    return pFtdiDevice;
}



}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_FTDIHW_H
