// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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


#include <nidas/core/SampleClientList.h>
#include <algorithm>

using namespace nidas::core;

SampleClientList::SampleClientList(const SampleClientList& cl):
    _clistLock(),_clients()
{
    cl.lock();
    _clients = cl._clients;
    cl.unlock();
}

SampleClientList& SampleClientList::operator=(const SampleClientList& rhs)
{
    if (this != &rhs) {
        rhs.lock();
        lock();
        _clients = rhs._clients;
        unlock();
        rhs.unlock();
    }
    return *this;
}

void SampleClientList::add(SampleClient* client)
{
    // prevent being added twice
    lock();
    std::list<SampleClient*>::iterator li =
        std::find(_clients.begin(),_clients.end(),client);
    if (li == _clients.end()) _clients.push_back(client);
    unlock();
}

void SampleClientList::remove(SampleClient* client)
{
    lock();
    std::list<SampleClient*>::iterator li =
        std::find(_clients.begin(),_clients.end(),client);
    if (li != _clients.end()) _clients.erase(li);
    unlock();
}
  
bool SampleClientList::empty() const
{
    lock();
    bool i = (signed) _clients.empty();
    unlock();
    return i;
}
  
void SampleClientList::removeAll()
{
    lock();
    _clients.clear();
    unlock();
}
