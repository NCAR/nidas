/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/ReadDerived.cc $
 ********************************************************************
*/

#include <nidas/core/ReadDerived.h>

#include <sstream>
#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
ReadDerived * ReadDerived::_instance = 0;
/* static */
nidas::util::Mutex ReadDerived::_instanceMutex;


ReadDerived::ReadDerived() : _tas(0), _alt(0), _radarAlt(0)
{
  _udp = new nidas::util::DatagramSocket(58802);
}

ReadDerived::~ReadDerived()
{
  _udp->close();
  delete _udp;
}

void ReadDerived::readData() throw(n_u::IOException,n_u::ParseException)
{
  char buffer[5000];
  size_t nBytes = _udp->recv(buffer, 5000, 0);

  if (nBytes == 0)
    return;

  parseIWGADTS(buffer);

  notifyClients();

}

bool ReadDerived::parseIWGADTS(char buffer[])
	throw(n_u::ParseException)
{
  if (memcmp(buffer, "IWG1", 4))
    return false;

  _lastUpdate = time(0);

  char *p = buffer;

  // Alt is the 3rd parameter.
  for (int i = 0; p && i < 4; ++i)
    p = strchr(p, ',')+1;

  if (!p) throw n_u::ParseException("cannot parse altitude",buffer);

  _alt = atof(p);


  // Radar Alt is the 5th parameter.
  for (int i = 0; p && i < 2; ++i)
    p = strchr(p, ',')+1;

  if (p)
    _radarAlt = atof(p);

  // True airspeed is the 7th parameter.
  for (int i = 0; p && i < 2; ++i)
    p = strchr(p, ',')+1;

  if (p)
    _tas = atof(p);

  return true;
}

ReadDerived * ReadDerived::getInstance()
{
  if (!_instance)
  {
    n_u::Synchronized autosync(_instanceMutex);
    if (!_instance)
      _instance = new ReadDerived();
  }
  return _instance;
}

void ReadDerived::addClient(DerivedDataClient * clnt)
{
    // prevent being added twice
    removeClient(clnt);
    _clientMutex.lock();
    _clients.push_back(clnt);
    _clientMutex.unlock();
}

void ReadDerived::removeClient(DerivedDataClient * clnt)
{
  std::list<DerivedDataClient*>::iterator li;
  _clientMutex.lock();
  for (li = _clients.begin(); li != _clients.end(); ) {
    if (*li == clnt) li = _clients.erase(li);
    else ++li;
  }
  _clientMutex.unlock();
}
void ReadDerived::notifyClients()
{

  /* make a copy of the list and iterate over the copy */
  _clientMutex.lock();
  list<DerivedDataClient*> tmp = _clients;
  _clientMutex.unlock();

  std::list<DerivedDataClient*>::iterator li;
  for (li = tmp.begin(); li != tmp.end(); ) {
    DerivedDataClient *clnt = *li;
    clnt->derivedDataNotify(this);
  }
}
