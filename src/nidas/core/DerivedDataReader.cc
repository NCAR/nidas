/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>   // HAVE_PPOLL

#include <nidas/core/DerivedDataReader.h>
#include <nidas/core/DerivedDataClient.h>
#include <nidas/core/Sample.h>
#include <nidas/util/Socket.h>
#include <nidas/util/Logger.h>

#include <sstream>
#include <iostream>
#include <cstdlib> // atof()
#include <unistd.h> // usleep()

#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
DerivedDataReader * DerivedDataReader::_instance = 0;

/* static */
nidas::util::Mutex DerivedDataReader::_instanceMutex;

DerivedDataReader::DerivedDataReader(const n_u::SocketAddress& addr):
    n_u::Thread("DerivedDataReader"),
    _clientMutex(),_clients(),_saddr(addr.clone()),
    _tas(floatNAN), _at(floatNAN), _ps(floatNAN),
    _lat(floatNAN), _lon(floatNAN), _alt(floatNAN),
    _radarAlt(floatNAN), _thdg(floatNAN),
    _grndSpd(floatNAN),_parseErrors(0),_errorLogs(0), _fields()

{
    unblockSignal(SIGUSR1);
    blockSignal(SIGUSR1);

    // field numbers should be in increasing order
    _fields.push_back(IWG1_Field(1,&_lat));       // latitude is 1st field after timetag
    _fields.push_back(IWG1_Field(2,&_lon));       // longitude is 2nd field after timetag
    _fields.push_back(IWG1_Field(3,&_alt));       // altitude is 3rd field after timetag
    _fields.push_back(IWG1_Field(6,&_radarAlt));  // radar altitude is 6th field
    _fields.push_back(IWG1_Field(7,&_grndSpd));   // ground speed is 7th field
    _fields.push_back(IWG1_Field(8,&_tas));       // true airspeed is 8th field
    _fields.push_back(IWG1_Field(12,&_thdg));     // true heading is 12th field
    _fields.push_back(IWG1_Field(19,&_at));       // ambient temperature is 19th field
    _fields.push_back(IWG1_Field(22,&_ps));       // static pressure is 22nd field
}

DerivedDataReader::~DerivedDataReader()
{
    delete _saddr;
}

void DerivedDataReader::interrupt()
{
    try {
        kill(SIGUSR1);
    }
    catch(const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
}

int DerivedDataReader::run() throw(nidas::util::Exception)
{
    char buffer[1024];
    n_u::DatagramPacket packet(buffer,sizeof(buffer)-1);

    n_u::DatagramSocket usock;
    bool bound = false;

    // get the existing signal mask
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);

    // unblock SIGUSR1 in pselect
    sigdelset(&sigmask,SIGUSR1);

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd = usock.getFd();
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set readfds;
    FD_ZERO(&readfds);
#endif

    for (;;) {
        if (isInterrupted()) break;
        try {
            if (!bound) {
                usock.bind(*_saddr);
                bound = true;
            }

#ifdef HAVE_PPOLL
            if (::ppoll(&fds,1,NULL,&sigmask) < 0) {
                throw n_u::IOException(usock.getLocalSocketAddress().toAddressString(),"ppoll",errno);
            }
            if (fds.revents & POLLERR)
                throw n_u::IOException(usock.getLocalSocketAddress().toAddressString(),"POLLERR",errno);
#ifdef POLLRDHUP
            if (fds.revents & (POLLHUP | POLLRDHUP))
#else
            if (fds.revents & (POLLHUP)) 
#endif
                NLOG(("%s: POLLHUP",usock.getLocalSocketAddress().toAddressString().c_str()));

            if (!fds.revents & POLLIN) continue;
#else
            int fd = usock.getFd();
            FD_SET(fd,&readfds);
            int nfd = ::pselect(fd+1,&readfds,NULL,NULL,0,&sigmask);
            if (nfd < 0) throw n_u::IOException(
                usock.getLocalSocketAddress().toAddressString(),"pselect",errno);
#endif

            usock.receive(packet);
            buffer[packet.getLength()] = 0;  // null terminate if nec.
            int nerr = _parseErrors;
            if (parseIWGADTS(buffer) > 0) notifyClients();
            if (_parseErrors != nerr && !(_errorLogs++ % 30))
              WLOG(("DerivedDataReader parse exception #%d, buffer=%s\n", _parseErrors,buffer));

    // DLOG(("DerivedDataReader: alt=%f,radalt=%f,tas=%f,at=%f ",_alt,_radarAlt,_tas,_at));
        }
        catch(const n_u::IOException& e) {
            // if we've been interrupted don't report error.
            // isInterrupted() will also be true
            if (e.getErrno() == EINTR) break;
            PLOG(("DerivedDataReader: ") << usock.getLocalSocketAddress().toAddressString() << ": " << e.what());
            // if for some reason we're getting a mess of errors
            // on the socket, take a nap, rather than get in a tizzy.
	    usleep(USECS_PER_SEC/2);
        }
    }
    usock.close();
    return RUN_OK;
}

/*
 * return the number of requested comma-delimited fields that were found.
 * _parseErrors will be incremented if the number of expected fields are not found,
 * or if the contents of a field can't be read with a scanf %f.
 *
 * TODO: make this into a generic, delimited-field parser, adding additional control:
 * 1. provide some user control of whether fields are set to floatNAN in the following situations:
 *   * if a field is missing, i.e. nothing or only spaces between the delimiters: "IWG1,99,,"
 *   * if trailing fields are missing:  "IWG1,99,2,3" and one wants the 7th field
 *   * junk between the delimiters:  "IWG1,99,quack,3"
 *   Some may want a field to keep its previous value in the above situations.
 * 2. overload this method to return the const char* after the parsing?
 *
 */
int DerivedDataReader::parseIWGADTS(const char* buffer)
{
    int nfields = 0;
    unsigned int ifield = 0;
    if (memcmp(buffer, "IWG1", 4)) return nfields;

    const char *p = buffer;
    float val;

    // skip comma after IWG1
    if (!(p = strchr(p, ','))) {
        _parseErrors++;
        return ifield;
    }
    p++;

    for ( ; ifield < _fields.size(); ifield++) {
        int nf = _fields[ifield].nf;
        for ( ; nfields < nf; ) {
            if (!(p = strchr(p, ','))) {
                _parseErrors++;
                for (unsigned int i = ifield ; i < _fields.size(); i++) *_fields[i].fp = floatNAN;
                return ifield;
            }
            p++; nfields++;
        }
        if (*p == ',' || *p == '\0') {      // empty field, but aren't checking for whitespace
            *_fields[ifield].fp = floatNAN;
        }
        else {
            if (sscanf(p,"%f",&val) == 1)
                *_fields[ifield].fp = val;
            else {
                *_fields[ifield].fp = floatNAN;
                _parseErrors++;
            }
        }
    }
    return ifield;
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
        if (_instance) {
#ifdef DOUBLE_CHECK_JOIN
            try {
                if (_instance->isRunning()) _instance->cancel();
                if (!_instance->isJoined()) _instance->join();
            }
            catch(const n_u::Exception& e) {
                PLOG(("DerivedDataReader: ") << "cancel/join:" << e.what());
            }
#endif
            delete _instance;
            _instance = 0;
        }
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
