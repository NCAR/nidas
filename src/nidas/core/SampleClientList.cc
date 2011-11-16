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
