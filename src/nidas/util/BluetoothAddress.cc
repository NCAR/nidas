// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ********************************************************************
 */

#ifdef HAS_BLUETOOTHRFCOMM_H

#include <nidas/util/BluetoothAddress.h>
#include <nidas/util/IOException.h>

#include <sys/socket.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <iostream>

using namespace nidas::util;
using namespace std;

namespace n_u = nidas::util;

/* static */
::regex_t* BluetoothAddress::_addrPreg = 0;

/* static */
nidas::util::Mutex BluetoothAddress::_staticMutex;

/* static */
map<string,BluetoothAddress> BluetoothAddress::_addrMap;

/* static */
BluetoothAddress BluetoothAddress::getByName(const std::string& hostname)
    throw(UnknownHostException)
{
    {
        n_u::Autolock autolock(_staticMutex);

        // Check if regular expression needs compiling
        if (!_addrPreg) {
            int regstatus;
            const char *addrRE = "^[0-9A-Fa-f]{1,2}(:[0-9A-Fa-f]{1,2}){5}";
            _addrPreg = new ::regex_t;
            if ((regstatus = ::regcomp(_addrPreg,addrRE,REG_EXTENDED|REG_NOSUB)) != 0) {
                char regerrbuf[64];
                ::regerror(regstatus,_addrPreg,regerrbuf,sizeof regerrbuf);
                delete _addrPreg; _addrPreg = 0;
                throw n_u::UnknownHostException(hostname + ": " + string("regcomp: ") + regerrbuf + ": " +
                        addrRE);
            }
        }
    }

    bdaddr_t bdaddr = bdaddr_t();

    // If hostname matches hex address format: xx:xx:xx:xx:xx:xx use str2ba.
    if (::regexec(_addrPreg,hostname.c_str(),0,0,0) == 0) {
        ::str2ba(hostname.c_str(),&bdaddr); // always returns 0
        return BluetoothAddress(&bdaddr);
    }

    // If hostname is in our cache, return previously found address.
    // TODO: expire the cache after a period of time. Would be nice to
    // eventually support expiring the cache to support a changed
    // Bluetooth "friendly" name.
    map<string,BluetoothAddress>::const_iterator mi = _addrMap.find(hostname);
    if (mi != _addrMap.end()) return mi->second;

    int dev_id = hci_get_route(NULL);
    if (dev_id < 0) {
        n_u::IOException e(hostname,"hci_get_route",errno);
        throw n_u::UnknownHostException(e.what());
    }
    int sock = hci_open_dev(dev_id);
    if (sock < 0) {
        n_u::IOException e(hostname,"hci_open_dev",errno);
        throw n_u::UnknownHostException(e.what());
    }

    int len = 8;       // inquiry lasts for len * 1.28 seconds
    const int max_rsp = 255;
    inquiry_info ii[max_rsp];
    inquiry_info *iip = ii;
    // cerr << "sizeof(ii)=" << sizeof(ii) << endl;
    memset(ii,0,sizeof(ii));

    int flags = IREQ_CACHE_FLUSH;
    int num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &iip, flags);
    if( num_rsp < 0 ) {
        n_u::IOException e(hostname,"hci_inquiry",errno);
        ::close(sock);
        throw n_u::UnknownHostException(e.what());
    }

    n_u::Autolock autolock(_staticMutex);
    for (int i = 0; i < num_rsp; i++) {
        char name[248];
        bdaddr_t bdaddr = bdaddr_t();
        memcpy(&bdaddr,&ii[i].bdaddr,sizeof(bdaddr));
#ifdef DEBUG
        memset(&bdaddr,0,sizeof(bdaddr));
        ::ba2str(&bdaddr,name);
        cerr << "i=" << i << ", bdaddr=" << name << endl;

        for (int j = 0; j < 6; j++) {
            bdaddr.b[j] = ii[i].bdaddr.b[j];
            cerr << "j=" << j << hex << (int) bdaddr.b[j] << dec << endl;
        }

        // bdaddr = ii[i].bdaddr;
        // ::ba2str(&ii[i].bdaddr,name);
        ::ba2str(&bdaddr,name);
        cerr << "i=" << i << ", bdaddr=" << name << endl;
        memset(name,0,sizeof(name));
#endif
        name[0] = 0;
        if (hci_read_remote_name(sock, &bdaddr, sizeof(name), name, 0) >= 0) {
            _addrMap[name] = BluetoothAddress(&bdaddr);
            // cerr << "name=" << name << endl;
        }
    }
    ::close(sock);

    mi = _addrMap.find(hostname);
    if (mi != _addrMap.end()) return mi->second;
    return BluetoothAddress();
}

/* static */
std::string BluetoothAddress::getHostName(const BluetoothAddress& addr) throw()
{
    char straddr[32];
    ::ba2str(&addr._bdaddr,straddr);
    return straddr;
}

BluetoothAddress::BluetoothAddress(): _bdaddr()
{
}

BluetoothAddress::BluetoothAddress(const bdaddr_t* a):
	_bdaddr(*a)
{
}

std::string BluetoothAddress::getHostName() const
{
    char straddr[32];
    ::ba2str(&_bdaddr,straddr);
    return straddr;
}

bool BluetoothAddress::operator < (const BluetoothAddress& x) const
{
    // use bacmp from bluetooth.h, like memcmp.
    return ::bacmp(&_bdaddr,&x._bdaddr) < 0;
}

bool BluetoothAddress::operator == (const BluetoothAddress& x) const
{
    return ::bacmp(&_bdaddr,&x._bdaddr) == 0;
}

bool BluetoothAddress::operator != (const BluetoothAddress& x) const
{
    return ::bacmp(&_bdaddr,&x._bdaddr) != 0;
}

#endif
