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
#include <list>
#include <ftdi.h>

#include "IOException.h"
#include "auto_ptr.h"
#include "Logger.h"

struct libusb_device;

namespace nidas { namespace util {

class FtdiList
{
public:
    FtdiList();
    ~FtdiList();

    /// List type storing "Context" objects
    typedef std::list<libusb_device*> ListType;
    /// Iterator type for the container
    typedef ListType::iterator iterator;
    /// Const iterator type for the container
    typedef ListType::const_iterator const_iterator;
    /// Reverse iterator type for the container
    typedef ListType::reverse_iterator reverse_iterator;
    /// Const reverse iterator type for the container
    typedef ListType::const_reverse_iterator const_reverse_iterator;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    reverse_iterator rbegin();
    reverse_iterator rend();
    const_reverse_iterator rbegin() const;
    const_reverse_iterator rend() const;

    ListType::size_type size() const;
    bool empty() const;
    void clear();

    void push_back(libusb_device* element);
    void push_front(libusb_device* element);

    iterator erase(iterator pos);
    iterator erase(iterator beg, iterator end);

private:
    class Private
    {
    public:
        Private() : list() {}

        ~Private() {}

        std::list<libusb_device*> list;
    };
    auto_ptr<Private> d;
};


/*
 ********************************************************************
 ** Class FtdiHW captures the serial board FTDI chips so that there is
 ** a central place to manage these devices.
 **
 ** Class FtdiHW is a singleton.
 **
 **
 ********************************************************************
 */
class FtdiHW
{
public:
    /*
     *  Static method to instantiate and/or return the singleton FtdiHW object.
     */
    static FtdiHW& getFtdiHW();

    /*
     *  Method to find all the FTDI FT4232H devices in the system and put them into
     *  a list<>. Can be used to rescan an already instantiated FtdiHW singleton.
     */
    void findFtdiDevices();

    /*
     *  Gets a reference to the FtdiList object
     */
    FtdiList& getFtdiDeviceList() {return _devList;}

    /*
     *  Gets a copy of the serial number string.
     */
    std::string getFtdiSerialNumber() {return _serialNo;}

private:

    /*
     *  Constructor for the singleton object. This method calls findFtdiDevices() and then
     *  gets the first device found to extract the serial board serial number.
     */
    FtdiHW();

    static const int _VENDOR = 0x403;
    static const int _PRODUCT = 0x6011;

    static FtdiHW* _pFtdiHW;

    std::string _serialNo;
    FtdiList _devList;

    /*
     *  No copying
     */
    FtdiHW(const FtdiHW& rRight);
    FtdiHW& operator=(const FtdiHW& rRight);
    FtdiHW& operator=(FtdiHW& rRight);
};

/*
 *  Generic FTDI device aimed at a particular interface on a device with an eeprom which has been programmed
 *  with specified manufacturer and product description strings. It is further specialized by specifying which
 *  FTDI interface/port is to be used for the operations performed by the methods.
 */
class FtdiDevice
{
public:
    /*
     *  FtdiDevice constructor.
     */
    FtdiDevice(const std::string vendor, const std::string product, ftdi_interface iface);

    /*
     *  Method finds the USB device which is described by the vendor and product strings. It
     *  then stores the bus and device addresses in the object for later use re-opening the device.
     */
    void find(std::string vendor, std::string product);

    /*
     *  Helper method to return the device found status
     */
    bool deviceFound() {return _foundDevice;}

    /*
     *  Used to set the operational mode of the device, usually bitbang mode.
     */
    bool setMode(unsigned char mask, unsigned char mode) {
        return (ftdi_set_bitmode(_pContext, mask, mode) == 0);
    }

    /*
     *  Used to set the device interface, upon which the operations in this class operate.
     */
    bool setInterface(ftdi_interface iface) {
        return (ftdi_set_interface(_pContext, iface) == 0);
    }

    // safe FT4232H open - must bracket all bitbang type operations!!!
    // returns w/o action, if already open.
    void open();

    // safe FT4232H close - must bracket all FTDI bitbang type operations!!!
    // returns w/o action, if already closed.
    void close();

    /*
     *  Helper method to return the open status of the device.
     */
    bool isOpen()
    {
        return  _pContext->usb_dev != 0;
    }

    /*
     *  Method opens the device, reads the pre-selected interface and
     *  returns the value of the port pins last written, then closes the device.
     */
    unsigned char readInterface();

    /*
     *  Method opens the device, writes to the pre-selected interface,
     *  and then closes the device.
     */
    void writeInterface(unsigned char pins);

    /*
     * returns the last textual status of the last operation.
     */
    std::string error_string()
    {
        return std::string(_pContext->error_str);
    }

private:
    ftdi_interface _interface;
    ftdi_context* _pContext;
    int _busAddr;
    int _devAddr;
    bool _foundDevice;
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_AUTOCONFIGHW_H
