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

SampleClientList::SampleClientList(const SampleClientList& cl)
{
    cl.lock();
    clients = cl.clients;
    cl.unlock();
}

SampleClientList& SampleClientList::operator=(const SampleClientList& cl)
{
    if (this != &cl) {
        cl.lock();
        lock();
        clients = cl.clients;
        unlock();
        cl.unlock();
    }
    return *this;
}

void SampleClientList::add(SampleClient* client)
{
    // prevent being added twice
    lock();
    std::list<SampleClient*>::iterator li =
        std::find(clients.begin(),clients.end(),client);
    if (li == clients.end()) clients.push_back(client);
    unlock();
}

void SampleClientList::remove(SampleClient* client)
{
    lock();
    std::list<SampleClient*>::iterator li =
        std::find(clients.begin(),clients.end(),client);
    if (li != clients.end()) clients.erase(li);
    unlock();
}
  
int SampleClientList::size() const
{
    lock();
    int i = (signed) clients.size();
    unlock();
    return i;
}
  
void SampleClientList::removeAll()
{
    lock();
    clients.clear();
    unlock();
}
