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

#include <libusb-1.0/libusb.h>

#include "Logger.h"
#include "FtdiHW.h"


namespace nidas { namespace util {

FtdiHW* FtdiHW::_pFtdiHW = 0;

FtdiHW& FtdiHW::getFtdiHW()
{
    if (!_pFtdiHW) {
        DLOG(("FtdiHW::getFtdiHW() instantiating new object..."));
        _pFtdiHW = new FtdiHW();
    }

    if (!_pFtdiHW) {
        throw Exception("Failed to create FtdiHW singleton object!!");
    }
    return *_pFtdiHW;
}


FtdiHW::FtdiHW() : _serialNo(""), _devList()
{
    DLOG(("FtdiHW(): start..."));
    findFtdiDevices();
    FtdiList::iterator iter = _devList.begin();
    if (iter != _devList.end()) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(*iter, &desc) > -1) {
            ftdi_context* pContext = ftdi_new();
            static const int SERIAL_BUF_SIZE = 50;
            char serialBuf[SERIAL_BUF_SIZE];
            memset(serialBuf, 0, SERIAL_BUF_SIZE);
            if (!ftdi_usb_get_strings(pContext, *iter, NULL, 0, NULL, 0, serialBuf, SERIAL_BUF_SIZE)) {
                _serialNo.append(serialBuf);
                DLOG(("FtdiHW(): found serial number: ") << _serialNo);
            }
            else
                DLOG(("FtdiHW::FtdiHW(): Failed to find serial number..."));
        }
        else {
            DLOG(("FtdiHW(): failed to find FTDI USB devices..."));
        }
    }
    else {
        DLOG(("FtdiHW(): failed to find FTDI USB devices..."));
    }

    DLOG(("FtdiHW(): ctor end..."));
}

void FtdiHW::findFtdiDevices()
{
    libusb_device **devs;
    libusb_init(NULL);
    ssize_t numDevices = libusb_get_device_list(NULL, &devs);
    DLOG(("Found ") << numDevices << " USB devices to check...");
    if (numDevices > 0) {
        for (int i=0; i<numDevices; ++i) {
            libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(devs[i], &desc) > -1) {
                DLOG(("Checking USB device #") << i << " at ");
                DLOG(("    Bus:") << (int)libusb_get_bus_number(devs[i]) << ", Port:" << (int)libusb_get_device_address(devs[i]));
                DLOG(("    Vendor:Product: ") << std::hex << "0x" << desc.idVendor << ":" << "0x" << desc.idProduct << std::dec);
                // only care about FTDI 4232H parts
                if (desc.idVendor == 0x0403 && desc.idProduct == 0x6011) {
                    DLOG(("    Found an FTDI chip, adding to the list..."));
                    _devList.push_back(devs[i]);
                }
            }
        }
        if (!_devList.size()) {
            DLOG(("FtdiHW::FtdiHW(): Found no FTDI devices"));
        }
    }
    else {
        DLOG(("FtdiHW::FtdiHW(): No USB devices to scan for FTDI devices..."));
    }
}

FtdiDevice::FtdiDevice(const std::string vendor, const std::string product, ftdi_interface iface)
: _interface(iface), _pContext(ftdi_new()), _busAddr(0), _devAddr(0), _foundDevice(false)
{
    if (!_pContext) {
        throw IOException("FtdiDevice::FtdiDevice()", ": Failed to allocate ftdi_context object.");
    }

    DLOG(("FtdiDevice(): testing to see if device exists..."));
    find(vendor, product);
    if (deviceFound()) {
        DLOG(("FtdiDevice(): set interface..."));
        if (setInterface(_interface)) {
            DLOG(("Opening device and setting bitbang mode"));
            open();
            if (setMode(0xFF, BITMODE_BITBANG)) {
                DLOG(("FtdiDevice(): Successfully set mode to bitbang: "));
            }
            else {
                DLOG(("FtdiDevice(): failed to set mode to bitbang: ") << error_string());
            }
            DLOG(("FtdiDevice(): Closing device after bitbang mode operation"));
            close();
        }
        else {
            DLOG(("FtdiDevice(): failed to set the interface...") << error_string());
        }
    }
}

void FtdiDevice::find(std::string vendorStr, std::string productStr)
{
    bool foundIt = false;
    FtdiList& devList = FtdiHW::getFtdiHW().getFtdiDeviceList();
    for (FtdiList::iterator it = devList.begin(); it != devList.end(); it++) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(*it, &desc) > -1) {
            DLOG(("Checking USB device at "));
            DLOG(("    Bus:") << (int)libusb_get_bus_number(*it) << ", Port:" << (int)libusb_get_device_address(*it));
            DLOG(("    Vendor:Product: ") << std::hex << "0x" << desc.idVendor << ":" << "0x" << desc.idProduct << std::dec);
            // only care about FTDI 4232H parts
            if (desc.idVendor == 0x0403 && desc.idProduct == 0x6011) {
                DLOG(("    Found an FTDI chip..."));
                libusb_device_handle* pHdl = 0;
                int libusbOpenStatus = libusb_open(*it, &pHdl);
                if (libusbOpenStatus == LIBUSB_SUCCESS) {
                    uint8_t descrString[256];
                    memset(descrString, 0, 256);
                    libusb_get_string_descriptor_ascii(pHdl, desc.iManufacturer, descrString, 256);
                    std::string manDescStr = (char*)descrString;
                    DLOG(("    Manufacturer: ") << manDescStr);
                    if (manDescStr == vendorStr) {
                        memset(descrString, 0, 256);
                        libusb_get_string_descriptor_ascii(pHdl, desc.iProduct, descrString, 256);
                        std::string prodDescStr = (char*)descrString;
                        DLOG(("    Product: ") << prodDescStr);
                        if (prodDescStr == productStr) {
                            foundIt = true;
                            DLOG(("Found ") << productStr << " FTDI device at");
                            DLOG(("    Bus:") << (int)libusb_get_bus_number(*it) << ", Port:" << (int)libusb_get_device_address(*it));
                            DLOG(("    Vendor:Product: ") << std::hex << "0x" << desc.idVendor << ":" << "0x" << desc.idProduct << std::dec);
                            DLOG(("    Manufacturer: ") << manDescStr << ", Product: " << prodDescStr);
                            _busAddr = libusb_get_bus_number(*it);
                            _devAddr = libusb_get_device_address(*it);
                        }
                        else {
                            DLOG(("Not a product candidate: "));
                            DLOG(("    Manufacturer: ") << manDescStr << ", Product: " << prodDescStr);
                        }
                    }
                    else {
                        DLOG(("Not a manufacturer candidate: "));
                        DLOG(("    Manufacturer: ") << manDescStr);
                    }
                }
                else {
                    DLOG(("    <<<*** Failed to open USB device. Error: ") << libusbOpenStatus << " ***>>>");
                }
                libusb_close(pHdl);
                if (foundIt) {
                    _foundDevice = true;
                    break;
                }
            }
        }
    }

    if (!foundIt) {
        DLOG(("FtdiDevice::find(): failed to find FTDI device with vendor: ") << vendorStr << ", and product" << productStr);
    }
}

void FtdiDevice::open()
{
    if (deviceFound()) {
        if (!isOpen()) {
            DLOG(("FtdiDevice::open(): Attempting to open FTDI device..."));
            int openStatus = ftdi_usb_open_bus_addr(_pContext, _busAddr, _devAddr);
            if (openStatus == 0) {
                DLOG(("FtdiDevice::open(): Successfully opened FTDI device..."));
            }
            else {
                DLOG(("FtdiDevice::open(): Failed to open FTDI device, trying reopen ") << error_string() << " open() status: " << openStatus);

            }
        }
        else {
            DLOG(("FtdiDevice::open(): FTDI device already open..."));
        }
    }
}

// safe FTDI close - must bracket all FTDI bitbang type operations!!!
// returns true if already closed or successfully closed, false if attempted close fails.
void FtdiDevice::close()
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

unsigned char FtdiDevice::readInterface()
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

void FtdiDevice::writeInterface(unsigned char pins)
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

FtdiList::FtdiList()
    : d( new Private() )
{
}

FtdiList::~FtdiList()
{
}

/**
* Return begin iterator for accessing the contained list elements
* @return Iterator
*/
FtdiList::iterator FtdiList::begin()
{
    return d->list.begin();
}

/**
* Return end iterator for accessing the contained list elements
* @return Iterator
*/
FtdiList::iterator FtdiList::end()
{
    return d->list.end();
}

/**
* Return begin iterator for accessing the contained list elements
* @return Const iterator
*/
FtdiList::const_iterator FtdiList::begin() const
{
    return d->list.begin();
}

/**
* Return end iterator for accessing the contained list elements
* @return Const iterator
*/
FtdiList::const_iterator FtdiList::end() const
{
    return d->list.end();
}

/**
* Return begin reverse iterator for accessing the contained list elements
* @return Reverse iterator
*/
FtdiList::reverse_iterator FtdiList::rbegin()
{
    return d->list.rbegin();
}

/**
* Return end reverse iterator for accessing the contained list elements
* @return Reverse iterator
*/
FtdiList::reverse_iterator FtdiList::rend()
{
    return d->list.rend();
}

/**
* Return begin reverse iterator for accessing the contained list elements
* @return Const reverse iterator
*/
FtdiList::const_reverse_iterator FtdiList::rbegin() const
{
    return d->list.rbegin();
}

/**
* Return end reverse iterator for accessing the contained list elements
* @return Const reverse iterator
*/
FtdiList::const_reverse_iterator FtdiList::rend() const
{
    return d->list.rend();

}

/**
* Get number of elements stored in the list
* @return Number of elements
*/
FtdiList::ListType::size_type FtdiList::size() const
{
    return d->list.size();
}

/**
* Check if list is empty
* @return True if empty, false otherwise
*/
bool FtdiList::empty() const
{
    return d->list.empty();
}

/**
 * Removes all elements. Invalidates all iterators.
 * Do it in a non-throwing way and also make
 * sure we really free the allocated memory.
 */
void FtdiList::clear()
{
    ListType().swap(d->list);
}

/**
 * Appends a copy of the element as the new last element.
 * @param element Value to copy and append
*/
void FtdiList::push_back(libusb_device* element)
{
    d->list.push_back(element);
}

/**
 * Adds a copy of the element as the new first element.
 * @param element Value to copy and add
*/
void FtdiList::push_front(libusb_device* element)
{
    d->list.push_front(element);
}

/**
 * Erase one element pointed by iterator
 * @param pos Element to erase
 * @return Position of the following element (or end())
*/
FtdiList::iterator FtdiList::erase(iterator pos)
{
    return d->list.erase(pos);
}

/**
 * Erase a range of elements
 * @param beg Begin of range
 * @param end End of range
 * @return Position of the element after the erased range (or end())
*/
FtdiList::iterator FtdiList::erase(iterator beg, iterator end)
{
    return d->list.erase(beg, end);
}



}} // namespace nidas { namespace util {
