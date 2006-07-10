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

using namespace nidas::core;

SampleClientList::SampleClientList(const SampleClientList& cl) {
    cl.lock();
    clients = cl.clients;
    cl.unlock();
}

SampleClientList& SampleClientList::operator=(const SampleClientList& cl) {
    cl.lock();
    lock();
    clients = cl.clients;
    unlock();
    cl.unlock();
    return *this;
}

void SampleClientList::add(SampleClient* client) {
  // prevent being added twice
  remove(client);
  lock();
  clients.push_back(client);
  unlock();
}

void SampleClientList::remove(SampleClient* client) {
  lock();
  std::list<SampleClient*>::iterator li;
  for (li = clients.begin(); li != clients.end(); ) {
    if (*li == client) li = clients.erase(li);
    else ++li;
  }
  unlock();
}
  
int SampleClientList::size() const {
  lock();
  int i = (signed) clients.size();
  unlock();
  return i;
}
  
void SampleClientList::removeAll() {
  lock();
  clients.clear();
  unlock();
}
