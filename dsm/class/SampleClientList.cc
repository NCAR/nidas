/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#include <SampleClientList.h>

using namespace dsm;

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
  
void SampleClientList::removeAll() {
  lock();
  clients.clear();
  unlock();
}
