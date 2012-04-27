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

#include <list>

#include <nidas/util/Inet4NetworkInterface.h>
#include <nidas/util/Socket.h>

using namespace nidas::util;
using namespace std;

/* static */
Inet4NetworkInterface Inet4NetworkInterface::getInterface(const Inet4Address& addr)
        throw (IOException)
{
    // No match, check if addr is one of my interfaces
    list<Inet4NetworkInterface> ifaces;

    // The Socket constructor, getInterfaces() and close() can all
    // throw IOException.
    Socket tmpsock;
    try {
        ifaces = tmpsock.getInterfaces();
    }
    catch(const IOException& e) {
        tmpsock.close();
        throw;
    }
    tmpsock.close();

    list<Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
    for ( ; ii != ifaces.end(); ++ii) {
        Inet4NetworkInterface iface = *ii;
        if (iface.getAddress() == addr) return iface;
    }

    // not one of my interfaces, return an interface with a negative index.
    Inet4NetworkInterface iface;
    iface._index = -1;
    return iface;
}
