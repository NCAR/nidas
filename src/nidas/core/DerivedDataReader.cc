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
    _saddr(addr.clone()),_parseErrors(0),_errorLogs(0)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
    unblockSignal(SIGUSR1);

    _nparse = 6;        // how many fields to parse
    _fields = new IWG1_Fields[_nparse];

    int i = 0;
    _fields[i].nf =  3; _fields[i++].fp = &_alt;       // altitude is 3rd field after timetag
    _fields[i].nf =  6; _fields[i++].fp = &_radarAlt;  // radar altitude is 6th field
    _fields[i].nf =  7; _fields[i++].fp = &_grndSpd;   // ground speed is 7th field
    _fields[i].nf =  8; _fields[i++].fp = &_tas;       // true airspeed is 8th field
    _fields[i].nf = 12; _fields[i++].fp = &_thdg;      // true heading is 12th field
    _fields[i].nf = 19; _fields[i++].fp = &_at;        // ambient temperature is 19th field

    assert(i == _nparse);
    for (i = 0; i < _nparse; i++) *_fields[i].fp = floatNAN;
}

DerivedDataReader::~DerivedDataReader()
{
    delete _saddr;
    delete [] _fields;
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
            int nerr = _parseErrors;
            if (parseIWGADTS(buffer) > 0) notifyClients();
            if (_parseErrors != nerr && !(_errorLogs++ % 30))
              WLOG(("DerivedDataReader parse exception #%d, buffer=%s\n", _parseErrors,buffer));

    // DLOG(("DerivedDataReader: alt=%f,radalt=%f,tas=%f,at=%f ",_alt,_radarAlt,_tas,_at));
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

/*
 * return number of comma-delimited fields found, 0 if buffer doesn't start with "IWG1,"
 * _parseErrors will be incremented if the number of expected fields are not found,
 * or if the contents of a field can't * be read with scanf %f.
 */
int DerivedDataReader::parseIWGADTS(const char* buffer)
	throw(n_u::ParseException)
{
    int nfields = 0;
    if (memcmp(buffer, "IWG1", 4)) return nfields;

    const char *p = buffer;
    float val;

    // skip comma after IWG1
    if (!(p = strchr(p, ','))) {
        _parseErrors++;
        return nfields;
    }
    p++;

    for (int ip = 0; ip < _nparse; ip++) {
        int nf = _fields[ip].nf;
        for ( ; nfields < nf; ) {
            if (!(p = strchr(p, ','))) {
                _parseErrors++;
                for ( ; ip < _nparse; ip++) *_fields[ip].fp = floatNAN;
                return nfields;
            }
            p++; nfields++;
        }
        if (sscanf(p,"%f",&val) == 1) *_fields[ip].fp = val;
        else {
            *_fields[ip].fp = floatNAN;
            _parseErrors++;
        }
    }
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
