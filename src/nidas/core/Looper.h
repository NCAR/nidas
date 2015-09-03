// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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


#ifndef NIDAS_CORE_LOOPER_H
#define NIDAS_CORE_LOOPER_H

#include <nidas/core/LooperClient.h>

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

#include <sys/time.h>
#include <sys/select.h>
#include <assert.h>

#include <list>

namespace nidas { namespace core {

/**
 * Looper is a Thread that periodically loops, calling
 * the LooperClient::looperNotify() method of
 * LooperClients at the requested intervals.
 */
class Looper : public nidas::util::Thread {
public:

    Looper();

    /**
     * Add a client to the Looper whose
     * LooperClient::looperNotify() method should be
     * called every msec number of milliseconds.
     * @param msecPeriod Time period, in milliseconds.
     * Since the system nanosleep function is only precise
     * to about 10 milliseconds, and to reduce system load,
     * this value is rounded to the nearest 10 milliseconds.
     */
    void addClient(LooperClient *clnt,unsigned int msecPeriod)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Remove a client from the Looper.
     */
    void removeClient(LooperClient *clnt);

    /**
     * Thread function.
     */
    virtual int run() throw(nidas::util::Exception);

    /**
     * Utility function for finding greatest common divisor.
     */
    static int gcd(unsigned int a, unsigned int b);

private:

    void setupClientMaps();

    nidas::util::Mutex _clientMutex;

    std::map<unsigned int,std::set<LooperClient*> > _clientsByPeriod;

    std::map<int,std::list<LooperClient*> > _clientsByCntrMod;

    std::set<int> _cntrMods;

    unsigned int _sleepMsec;

};

}}	// namespace nidas namespace core

#endif
