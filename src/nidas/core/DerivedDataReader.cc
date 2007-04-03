/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/DerivedDataReader.cc $
 ********************************************************************
*/

#include <nidas/core/DerivedDataReader.h>
#include <nidas/core/DerivedDataClient.h>
#include <nidas/util/Logger.h>

#include <sstream>
#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
DerivedDataReader * DerivedDataReader::_instance = 0;

/* static */
nidas::util::Mutex DerivedDataReader::_instanceMutex;

DerivedDataReader::DerivedDataReader(const n_u::Inet4SocketAddress& addr)
    throw(n_u::IOException): n_u::Thread("DerivedDataReader"),
    _usock(addr),_tas(0), _alt(0), _radarAlt(0)
{
}

DerivedDataReader::~DerivedDataReader()
{
  _usock.close();
}

int DerivedDataReader::run() throw(nidas::util::Exception)
{

    for (;;) {
        if (isInterrupted()) break;
        try {
            readData();
        }
        catch(const n_u::IOException& e) {
            PLOG(("DerivedDataReader: ") << _usock.getLocalSocketAddress().toString() << ": " << e.what());
        }
        catch(const n_u::ParseException& e) {
            WLOG(("DerivedDataReader: ") << _usock.getLocalSocketAddress().toString() << ": " << e.what());
        }
    }
    return RUN_OK;
}

void DerivedDataReader::readData() throw(n_u::IOException,n_u::ParseException)
{
  char buffer[5000];
  n_u::DatagramPacket packet(buffer,sizeof(buffer)-1);

  _usock.receive(packet);
  if (packet.getLength() == 0) return;

  buffer[packet.getLength()] = 0;  // null terminate if nec.

  DLOG(("DerivedDataReader: ") << buffer);
  parseIWGADTS(buffer);

  notifyClients();

}

bool DerivedDataReader::parseIWGADTS(char buffer[])
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
  DLOG(("DerivedDataReader: alt=%f,radalt=%f,tas=%f, ",_alt,_radarAlt,_tas));

  return true;
}

DerivedDataReader * DerivedDataReader::createInstance(const n_u::Inet4SocketAddress & addr)
    throw(n_u::IOException)
{
  if (!_instance)
  {
    n_u::Synchronized autosync(_instanceMutex);
    if (!_instance)
      _instance = new DerivedDataReader(addr);
      _instance->start();
  }
  return _instance;
}

void DerivedDataReader::deleteInstance()
{
  if (!_instance)
  {
    n_u::Synchronized autosync(_instanceMutex);
    if (_instance)
      if (_instance->isRunning()) {
          _instance->interrupt();
          try {
              // _instance->cancel();
              // Send a SIGUSR1 signal, which should result in an
              // EINTR on the socket read.
              _instance->kill(SIGUSR1);
              _instance->join();
          }
          catch(const n_u::Exception& e) {
            PLOG(("DerivedDataReader: ") << "cancel/join:" << e.what());
          }
        }
      _instance = 0;
  }
}


DerivedDataReader * DerivedDataReader::getInstance()
{
  return _instance;
}

void DerivedDataReader::addClient(DerivedDataClient * clnt)
{
    // prevent being added twice
    removeClient(clnt);
    _clientMutex.lock();
    _clients.push_back(clnt);
    _clientMutex.unlock();
}

void DerivedDataReader::removeClient(DerivedDataClient * clnt)
{
  std::list<DerivedDataClient*>::iterator li;
  _clientMutex.lock();
  for (li = _clients.begin(); li != _clients.end(); ) {
    if (*li == clnt) li = _clients.erase(li);
    else ++li;
  }
  _clientMutex.unlock();
}
void DerivedDataReader::notifyClients()
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
