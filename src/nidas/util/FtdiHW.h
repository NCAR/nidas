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

struct libusb_device;

namespace nidas { namespace util {

enum FTDI_DEVICES {FTDI_ILLEGAL=-1, FTDI_I2C, FTDI_GPIO, FTDI_P0_P4, FTDI_P5_P7, NUM_FTDI_DEVICES};

/*
 *  FtdiDevice interface class
 */
class FtdiDeviceIF
{
public:
    virtual ~FtdiDeviceIF() {}
    virtual bool deviceFound() = 0;

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
     *  Method opens the device, reads the pre-selected interface and
     *  returns the value of the port pins last written, then closes the device.
     */
    virtual unsigned char readInterface() = 0;

    /*
     *  Method opens the device, writes to the pre-selected interface,
     *  and then closes the device.
     */
    virtual void writeInterface(unsigned char pins) = 0;

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
 *  NOTE: Class FtdiDevice is *not* intended to be used on its own, and so the constructor is not public.
 *        This is due to the fact that a single interface (i.e. byte-wide I/O port) can be assigned multiple
 *        responsibilities in different domains. For instance, one domain might be serial transceiver control
 *        while another domain might be scientific instrument power control. This is a very real use case in
 *        NIDAS for those systems which use the new EOL serial port boards.
 */
template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
class FtdiDevice : public FtdiDeviceIF
{
public:
    // static singleton getter
    static FtdiDevice<DEVICE, IFACE>* getFtdiDevice(const std::string manufStr = "UCAR", const std::string productStr = "GPIO");

    // static singleton destroyer
    static void deleteFtdiDevice();

    /*
     *  Helper method to return the device found status
     */
    virtual bool deviceFound();

    /*
     *  Used to set the operational mode of the device, usually bitbang mode.
     */
    virtual bool setMode(unsigned char mask, unsigned char mode);

    /*
     *  Used to set the device interface, upon which the operations in this class operate.
     */
    virtual bool setInterface(ftdi_interface iface);

    virtual ftdi_interface getInterface();

    // safe FT4232H open - must bracket all bitbang type operations!!!
    // returns w/o action, if already open.
    virtual void open();

    // safe FT4232H close - must bracket all FTDI bitbang type operations!!!
    // returns w/o action, if already closed.
    virtual void close();

    /*
     *  Helper method to return the open status of the device.
     */
    virtual bool isOpen();

    /*
     *  Method opens the device, reads the pre-selected interface and
     *  returns the value of the port pins last written, then closes the device.
     */
    virtual unsigned char readInterface();

    /*
     *  Method opens the device, writes to the pre-selected interface,
     *  and then closes the device.
     */
    virtual void writeInterface(unsigned char pins);

    /*
     * returns the last textual status of the last operation.
     */
    virtual std::string error_string();

private:
    static FtdiDevice<DEVICE, IFACE>* _pFtdiDevice;
    ftdi_context* _pContext;
    std::string _manufStr;
    std::string _productStr;
    bool _foundDevice;

    /*
     *  FtdiDevice constructor/destructor.
     *
     *  Make private so that class can be a singleton.
     */
    FtdiDevice(const std::string vendor, const std::string product);
    virtual ~FtdiDevice();

    /*
     *  No copying
     */
    FtdiDevice(const FtdiDevice& rRight);
    FtdiDevice& operator=(const FtdiDevice& rRight);
    FtdiDevice& operator=(FtdiDevice& rRight);
};

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiDevice<DEVICE, IFACE>* FtdiDevice<DEVICE, IFACE>::getFtdiDevice(const std::string manufStr, const std::string productStr)
{
    DLOG(("Getting FtdiDevice<%s> singleton", IFACE == INTERFACE_A ? "INTERFACE_A" : (IFACE == INTERFACE_B ? "INTERFACE_B" :
                                         (IFACE == INTERFACE_C ? "INTERFACE_C" : (IFACE == INTERFACE_D ? "INTERFACE_D" : "?")))));
    if (!_pFtdiDevice) {
        DLOG(("Constructing FtdiDevice<%s> singleton", IFACE == INTERFACE_A ? "INTERFACE_A" : (IFACE == INTERFACE_B ? "INTERFACE_B" :
                                             (IFACE == INTERFACE_C ? "INTERFACE_C" : (IFACE == INTERFACE_D ? "INTERFACE_D" : "?")))));
        _pFtdiDevice = new FtdiDevice<DEVICE, IFACE>(manufStr, productStr);
    }

    return _pFtdiDevice;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiDevice<DEVICE, IFACE>::deleteFtdiDevice()
{
    delete _pFtdiDevice;
}



template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiDevice<DEVICE, IFACE>::FtdiDevice(const std::string manufStr, const std::string productStr)
: _pContext(ftdi_new()), _manufStr(manufStr), _productStr(productStr), _foundDevice(false)
{
    if (!_pContext) {
        throw IOException("FtdiDevice::FtdiDevice()", ": Failed to allocate ftdi_context object.");
    }

    open();
    if (isOpen()) {
        DLOG(("FtdiDevice(): set interface..."));
        if (setInterface(IFACE)) {
            DLOG(("FtdiDevice(): successfully set the interface: "));
            DLOG(("FtdiDevice(): set bitbang mode"));
            if (setMode(0xFF, BITMODE_BITBANG)) {
                DLOG(("FtdiDevice(): Successfully set mode to bitbang"));
            }
            else {
                ILOG(("FtdiDevice(): failed to set mode to bitbang: ") << error_string());
            }
        }
        else {
            ILOG(("FtdiDevice(): failed to set the interface...") << error_string());
        }
    }
    else {
        ILOG(("FtdiDevice(): Failed to open the device!!"));
    }
    close();
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiDevice<DEVICE, IFACE>::~FtdiDevice()
{
    DLOG(("Destroying FtdiDevice..."));
//    close();
    ftdi_usb_close(_pContext);
    ftdi_free(_pContext);
    _pContext = 0;
}


template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiDevice<DEVICE, IFACE>::open()
{
    if (!isOpen()) {
        DLOG(("FtdiDevice::open(): Attempting to open FTDI device..."));
        int openStatus = ftdi_usb_open_desc(_pContext, (int)0x0403, (int)0x6011, _productStr.c_str(), 0);
        _foundDevice = !openStatus;
        if (deviceFound()) {
            // TODO: let's check the manuf string as well...

            DLOG(("FtdiDevice::open(): Successfully opened FTDI device..."));
        }
        else {
            DLOG(("FtdiDevice::open(): Failed to open FTDI device: ") << error_string() << " open() status: " << openStatus);
        }
    }
    else {
        DLOG(("FtdiDevice::open(): FTDI device already open..."));
    }
}

// safe FTDI close - must bracket all FTDI bitbang type operations!!!
// returns true if already closed or successfully closed, false if attempted close fails.
template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiDevice<DEVICE, IFACE>::close()
{
    if (deviceFound()) {
        if (isOpen()) {
            DLOG(("FtdiDevice::close(): Attempting to close FTDI device..."));
            if (!ftdi_usb_close(_pContext)) {
                DLOG(("FtdiDevice::close(): Successfully closed FTDI device..."));
            }
            else {
                DLOG(("FtdiDevice::close(): Failed to close FTDI device...") << error_string());
            }
        }
        else {
            DLOG(("FtdiDevice::close(): FTDI device already closed..."));
        }
    }
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
unsigned char FtdiDevice<DEVICE, IFACE>::readInterface()
{
    unsigned char pins = 0;
    if (deviceFound()) {
        open();
        if (isOpen()) {
            DLOG(("FtdiDevice::readInterface(): Successfully opened FTDI device"));
            if (!ftdi_read_pins(_pContext, &pins)) {
                DLOG(("FtdiDevice::readInterface(): Successfully read the device pins..."));
            }
            else {
                DLOG(("FtdiDevice::readInterface(): Failed to read the device pins...") << error_string());
                throw IOException(std::string("FtdiDevice::readInterface(): Could not read from interface: "),
                                  error_string());
            }
            close();
        }
        else {
            DLOG(("FtdiDevice::readInterface(): Failed to open FTDI device: ") << error_string());
            throw IOException(std::string("FtdiDevice::readInterface(): Failed to open the device"),
                              error_string());
        }
    }

    return pins;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
void FtdiDevice<DEVICE, IFACE>::writeInterface(unsigned char pins)
{
    if (deviceFound()) {
        open();
        if (ftdi_write_data(_pContext, &pins, 1) < 0) {
            throw IOException(std::string("FtdiDevice::writeInterface(): Could not write to interface: "),
                                          error_string());
        }
        close();
    }
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiDevice<DEVICE, IFACE>::deviceFound()
{
    return _foundDevice;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiDevice<DEVICE, IFACE>::setMode(unsigned char mask, unsigned char mode) {
    bool retval = true;
    if (deviceFound()) {
        retval = !ftdi_set_bitmode(_pContext, mask, mode);
    }

    return retval;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiDevice<DEVICE, IFACE>::setInterface(ftdi_interface iface) {
    bool retval = true;
    if (deviceFound()) {
        retval = !ftdi_set_interface(_pContext, iface);
    }
    return retval;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
ftdi_interface FtdiDevice<DEVICE, IFACE>::getInterface()
{
    return IFACE;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
bool FtdiDevice<DEVICE, IFACE>::isOpen()
{
    return  _pContext->usb_dev != 0;
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
std::string FtdiDevice<DEVICE, IFACE>::error_string()
{
    return std::string(_pContext->error_str);
}

template<FTDI_DEVICES DEVICE, ftdi_interface IFACE>
FtdiDevice<DEVICE, IFACE>* FtdiDevice<DEVICE, IFACE>::_pFtdiDevice = 0;

/*
 *  Helper method to get the right FtdiDevice singleton
 */

inline FtdiDeviceIF* getFtdiDevice(FTDI_DEVICES device, ftdi_interface iface) {
    FtdiDeviceIF* pFtdiDevice = 0;

    switch (device) {
    case FTDI_I2C:
		switch (iface) {
		case INTERFACE_A:
			DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_A> singleton..."));
			pFtdiDevice = FtdiDevice<FTDI_I2C, INTERFACE_A>::getFtdiDevice("UCAR", "I2C");
			break;
		case INTERFACE_B:
			DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_B> singleton..."));
			pFtdiDevice = FtdiDevice<FTDI_I2C, INTERFACE_B>::getFtdiDevice("UCAR", "I2C");
			break;
		case INTERFACE_C:
			DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_C> singleton..."));
			pFtdiDevice = FtdiDevice<FTDI_I2C, INTERFACE_C>::getFtdiDevice("UCAR", "I2C");
			break;
		case INTERFACE_D:
			DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_D> singleton..."));
			pFtdiDevice = FtdiDevice<FTDI_I2C, INTERFACE_D>::getFtdiDevice("UCAR", "I2C");
			break;
		default:
			ILOG(("getFtdiDevice(): Illegal ftdi_interface value!!"));
			throw InvalidParameterException("FtdiHW", "getFtdiDevice()", "Illegal ftdi_interface value!!");
			break;
		}
		break;

	    case FTDI_GPIO:
			switch (iface) {
			case INTERFACE_A:
				DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_A> singleton..."));
				pFtdiDevice = FtdiDevice<FTDI_GPIO, INTERFACE_A>::getFtdiDevice("UCAR", "GPIO");
				break;
			case INTERFACE_B:
				DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_B> singleton..."));
				pFtdiDevice = FtdiDevice<FTDI_GPIO, INTERFACE_B>::getFtdiDevice("UCAR", "GPIO");
				break;
			case INTERFACE_C:
				DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_C> singleton..."));
				pFtdiDevice = FtdiDevice<FTDI_GPIO, INTERFACE_C>::getFtdiDevice("UCAR", "GPIO");
				break;
			case INTERFACE_D:
				DLOG(("getFtdiDevice(): getting FtdiDevice<INTERFACE_D> singleton..."));
				pFtdiDevice = FtdiDevice<FTDI_GPIO, INTERFACE_D>::getFtdiDevice("UCAR", "GPIO");
				break;
			default:
				ILOG(("getFtdiDevice(): Illegal ftdi_interface value!!"));
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

#endif //NIDAS_UTIL_AUTOCONFIGHW_H
