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
#include <nidas/core/Sample.h>
#include <nidas/util/Socket.h>
#include <nidas/util/Logger.h>

#include <sstream>
#include <iostream>
#include <cstdlib> // atof()

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
DerivedDataReader * DerivedDataReader::_instance = 0;

/* static */
nidas::util::Mutex DerivedDataReader::_instanceMutex;

DerivedDataReader::DerivedDataReader(const n_u::SocketAddress& addr):
    n_u::Thread("DerivedDataReader"),
    _saddr(addr.clone()), _tas(floatNAN), _at(floatNAN), _alt(floatNAN),
    _radarAlt(floatNAN), _thdg(floatNAN), _parseErrors(0)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
    unblockSignal(SIGUSR1);
}

DerivedDataReader::~DerivedDataReader()
{
    delete _saddr;
}

int DerivedDataReader::run() throw(nidas::util::Exception)
{
    char buffer[1024];
    n_u::DatagramPacket packet(buffer,sizeof(buffer)-1);

    n_u::DatagramSocket usock;
    bool bound = false;

    for (;;) {
        if (isInterrupted()) break;
        try {
            if (!bound) {
                usock.bind(*_saddr);
                bound = true;
            }
            usock.receive(packet);
            buffer[packet.getLength()] = 0;  // null terminate if nec.
            if (parseIWGADTS(buffer)) notifyClients();
        }
        catch(const n_u::IOException& e) {
            // if interrupted don't report error. isInterrupted() will also be true
            if (e.getErrno() == EINTR) break;
            PLOG(("DerivedDataReader: ") << usock.getLocalSocketAddress().toString() << ": " << e.what());
            // if for some reason we're getting a mess of errors
            // on the socket, take a nap, rather than get in a tizzy.
            usleep(USECS_PER_SEC/2);
        }
        catch(const n_u::ParseException& e) {
            WLOG(("DerivedDataReader: ") << usock.getLocalSocketAddress().toString() << ": " << e.what());
            usleep(USECS_PER_SEC/2);
        }
    }
    usock.close();
    return RUN_OK;
}

bool DerivedDataReader::parseIWGADTS(const char* buffer)
	throw(n_u::ParseException)
{
    if (memcmp(buffer, "IWG1", 4))
      return false;

    const char *p = buffer;
    float val;

    // Alt is the 3rd parameter.
    for (int i = 0; p && i < 4; ++i)
      if ((p = strchr(p, ','))) p++;

    if (p) 
        if (sscanf(p,"%f",&val) == 1) _alt = val;

    // Radar Alt is the 6th parameter.
    for (int i = 0; p && i < 3; ++i)	// Move forward 3 places.
      if ((p = strchr(p, ','))) p++;

    if (p)
        if (sscanf(p,"%f",&val) == 1) _radarAlt = val;

    // True airspeed is the 8th parameter.
    for (int i = 0; p && i < 2; ++i) // Move forward 2 places.
      if ((p = strchr(p, ','))) p++;

    if (p)
        if (sscanf(p,"%f",&val) == 1) _tas = val;

    // True Heading is the 12th parameter.
    for (int i = 0; p && i < 4; ++i)      // Move forward 4 places.
      if ((p = strchr(p, ','))) p++;

    if (p)
        if (sscanf(p,"%f",&val) == 1) _thdg = val;

    // Ambient Temperature is the 19th parameter.
    for (int i = 0; p && i < 7; ++i)	// Move forward 7 places.
      if ((p = strchr(p, ','))) p++;

    if (p) {
        if (sscanf(p,"%f",&val) == 1) _at = val;
    }
    else
      if (!(_parseErrors++ % 100)) WLOG(("DerivedDataReader parse exception #%d, buffer=%s\n",
          _parseErrors,buffer));

    // DLOG(("DerivedDataReader: alt=%f,radalt=%f,tas=%f,at=%f ",_alt,_radarAlt,_tas,_at));

    return true;
}

DerivedDataReader * DerivedDataReader::createInstance(const n_u::SocketAddress & addr)
{
    if (!_instance) {
        n_u::Synchronized autosync(_instanceMutex);
        if (!_instance)
            _instance = new DerivedDataReader(addr);
        _instance->start();
    }
    return _instance;
}

void DerivedDataReader::deleteInstance()
{
    if (_instance) {
        n_u::Synchronized autosync(_instanceMutex);
        if (_instance && _instance->isRunning()) {
            _instance->interrupt();
            try {
                // Send a SIGUSR1 signal, which should result in an
                // EINTR on the socket read.
                _instance->kill(SIGUSR1);
                if (!_instance->isJoined()) _instance->join();
            }
            catch(const n_u::Exception& e) {
                PLOG(("DerivedDataReader: ") << "kill/join:" << e.what());
            }
        }
        delete _instance;
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
    for (li = tmp.begin(); li != tmp.end(); ++li) {
        DerivedDataClient *clnt = *li;
        clnt->derivedDataNotify(this);
    }
}
