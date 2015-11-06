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

#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <sstream>

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/IOException.h>
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::util;

MutexAttributes::MutexAttributes(): _attrs()
{
    ::pthread_mutexattr_init(&_attrs);
}

MutexAttributes::MutexAttributes(const MutexAttributes& x): _attrs(x._attrs)
{
    /* Not completely sure that a straight copy of _attrs
     * works, so we'll actually re-initialize _attrs. */
    ::pthread_mutexattr_init(&_attrs);

    // In theory, these setXXX should not throw exceptions, since
    // the attributes were OK in the original copy.
    try {
        setType(x.getType());
#ifdef PTHREAD_PRIO_INHERIT
        setPriorityCeiling(x.getPriorityCeiling());
        setProtocol(x.getProtocol());
#endif
        setPShared(x.getPShared());
    }
    catch (const Exception& e) {}
}

MutexAttributes::~MutexAttributes()
{
    ::pthread_mutexattr_destroy(&_attrs);
}

void MutexAttributes::setType(int val) throw(Exception)
{
    if (::pthread_mutexattr_settype(&_attrs,val)) {
        switch(errno) {
        case EINVAL:
        case ENOTSUP:
            {
            ostringstream ost;
            ost << "invalid value of " << val;
            throw InvalidParameterException("MutexAttributes","setType",ost.str());
            }
        default:
            throw Exception("MutexAttributes::setType",errno);
        }
    }
}
int MutexAttributes::getType() const
{
    int val;
    ::pthread_mutexattr_gettype(&_attrs,&val);
    return val;
}

#ifdef PTHREAD_PRIO_INHERIT
void MutexAttributes::setPriorityCeiling(int val) throw(Exception)
{
    int oldval;
    if (::pthread_mutexattr_setprioceiling(&_attrs,val)) {
        switch(errno) {
        case EINVAL:
            {
            ostringstream ost;
            ost << "invalid ceiling value of " << val;
            throw InvalidParameterException("MutexAttributes","setPriorityCeiling",ost.str());
            }
        default:
            throw Exception("MutexAttributes::setPriorityCeiling",errno);
        }
    }
}
int MutexAttributes::getPriorityCeiling() const
{
    int val;
    pthread_mutexattr_getprioceiling(&_attrs,&val);
    return val;
}
#endif

#ifdef PTHREAD_PRIO_INHERIT
void MutexAttributes::setProtocol(int val) throw(Exception)
{
    if (::pthread_mutexattr_setprotocol(&_attrs,val)) {
        switch(errno) {
        case EINVAL:
        case ENOTSUP:
            {
            ostringstream ost;
            ost << "invalid protocol value of " << val;
            throw InvalidParameterException("MutexAttributes","setProtocol",ost.str());
            }
        default:
            throw Exception("MutexAttributes::setProtocol",errno);
        }
    }
}
int MutexAttributes::getProtocol() const
{
    int val;
    ::pthread_mutexattr_getprotocol(&_attrs,&val);
    return val;
}
#endif

void MutexAttributes::setPShared(int val) throw(Exception)
{
    if (::pthread_mutexattr_setpshared(&_attrs,val)) {
        switch(errno) {
        case EINVAL:
        case ENOTSUP:
            {
            ostringstream ost;
            ost << "invalid pshared value of " << val;
            throw InvalidParameterException("MutexAttributes","setPShared",ost.str());
            }
        default:
            throw Exception("MutexAttributes::setPShared",errno);
        }
    }
}
int MutexAttributes::getPShared() const
{
    int val;
    ::pthread_mutexattr_getpshared(&_attrs,&val);
    return val;
}

RWLockAttributes::RWLockAttributes(): _attrs()
{
    ::pthread_rwlockattr_init(&_attrs);
}

RWLockAttributes::RWLockAttributes(const RWLockAttributes& x): _attrs(x._attrs)
{
    ::pthread_rwlockattr_init(&_attrs);

    // In theory, these setXXX should not throw exceptions, since
    // the attributes were OK in the original copy.
    try {
        setPShared(x.getPShared());
    }
    catch (const Exception& e) {}
}

RWLockAttributes::~RWLockAttributes()
{
    ::pthread_rwlockattr_destroy(&_attrs);
}

void RWLockAttributes::setPShared(int val) throw(Exception)
{
    if (::pthread_rwlockattr_setpshared(&_attrs,val)) {
        switch(errno) {
        case EINVAL:
        case ENOTSUP:
            {
            ostringstream ost;
            ost << "invalid pshared value of " << val;
            throw InvalidParameterException("RWLockAttributes","setPShared",ost.str());
            }
        default:
            throw Exception("RWLockAttributes::setPShared",errno);
        }
    }
}
int RWLockAttributes::getPShared() const
{
    int val;
    ::pthread_rwlockattr_getpshared(&_attrs,&val);
    return val;
}

Mutex::Mutex(int type) throw(): _p_mutex(),_attrs()
{
    /* Can fail:
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * EINVAL: invalid attributes
     * EBUSY: reinitialize an object already initialized (shouldn't happen)
     * We won't throw an exception here, and wait to throw it
     * when one tries a lock.
     */
    _attrs.setType(type);
    ::pthread_mutex_init (&_p_mutex,_attrs.ptr());
}

Mutex::Mutex(const MutexAttributes& attrs) throw(Exception) : _p_mutex(),_attrs(attrs)
{
    if (::pthread_mutex_init (&_p_mutex,_attrs.ptr()))
        throw Exception("Mutex(attrs) constructor",errno);
}

/*
 * Copy constructor. Creates a new, unlocked mutex.
 */
Mutex::Mutex(const Mutex& x) throw() :_p_mutex(),_attrs(x._attrs)
{
    ::pthread_mutex_init (&_p_mutex,_attrs.ptr());
}

Mutex::~Mutex() throw(Exception)
{
    int ret = 0;
    if ((ret = ::pthread_mutex_destroy(&_p_mutex))) {
        switch(ret) {
        case EBUSY:
// If you're getting terminate messages with Exception "~Mutex", then #define this
// and run in valgrind in order to figure out where it is happening.
#define DO_SEGFAULT_FOR_MUTEX_DEBUGGING
#ifdef DO_SEGFAULT_FOR_MUTEX_DEBUGGING
            {
            cerr << "~Mutex: Mutex is locked, forcing seg fault" << endl;
            int* p = 0;
            *p = 0;
            }
#endif
            throw Exception("~Mutex","Mutex is locked");
        default:
            throw IOException("~Mutex","destroy",errno);
        }
    }
}

pthread_mutex_t*
Mutex::ptr() {
     return &_p_mutex;
}


Cond::Cond() throw() : _p_cond(),_mutex() 
{
    /* Can fail:
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * EINVAL: invalid attributes (we're  not passing attributer)
     * EBUSY: reinitialize an object already initialized (shouldn't happen)
     * We won't throw an exception here, and wait to throw it
     * when one tries a lock.
     */
    ::pthread_cond_init (&_p_cond, 0);       // default attributes
}

Cond::Cond(const Cond& x) throw() : _p_cond(x._p_cond),_mutex(x._mutex)
{
    ::pthread_cond_init (&_p_cond, 0);
}

Cond::~Cond() throw(Exception)
{
    if (::pthread_cond_destroy (&_p_cond) && errno != EINTR) {
        switch(errno) {
        case EBUSY:
            throw Exception("~Cond",
                "Cond is being waited on by another thread");
        default:
            throw Exception("~Cond",errno);
        }
    }
}

void Cond::wait() throw(Exception)
{
    int res;
    
    // On thread cancellation, make sure the mutex is left unlocked.
    pthread_cleanup_push((void(*)(void*))::pthread_mutex_unlock,_mutex.ptr());

    if ((res = ::pthread_cond_wait (&_p_cond, _mutex.ptr())))
        throw Exception("Cond::wait",res);

    // thread was not canceled, don't want to unlock, so do a pop(0)
    pthread_cleanup_pop(0);
}

RWLock::RWLock() throw(): _p_rwlock(),_attrs()
{
    /* Can fail:
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * EPERM: insufficient privilege (not sure what this is about)
     * EBUSY: reinitialize an object already initialized (shouldn't happen)
     * We won't throw an exception here, and wait to throw it
     * when one tries a lock.
     */
    ::pthread_rwlock_init (&_p_rwlock, _attrs.ptr());
}

RWLock::RWLock(const RWLockAttributes& attrs) throw(Exception) :
    _p_rwlock(),_attrs(attrs)
{
    if (::pthread_rwlock_init (&_p_rwlock,_attrs.ptr()))
        throw Exception("RWLock(attrs) constructor",errno);
}

/*
 * Copy constructor. Creates a new, unlocked rwlock.
 */
RWLock::RWLock(const RWLock& x) throw() :
    _p_rwlock(x._p_rwlock),_attrs(x._attrs)
{
      ::pthread_rwlock_init (&_p_rwlock, _attrs.ptr());
}

RWLock::~RWLock() throw(Exception)
{
    if (::pthread_rwlock_destroy(&_p_rwlock)) {
        switch(errno) {
        case EBUSY:
            throw Exception("~RWLock","RWLock is locked");
        default:
            throw Exception("~RWLock",errno);
        }
    }
}

pthread_rwlock_t*
RWLock::ptr() {
     return &_p_rwlock;
}

Multisync::Multisync(int n):
 _co(), _n(n), _count(0), debug(0) {

  char* debug_env = getenv("MULTISYNC_DEBUG");

  if(debug_env)
    debug = 1;

}

void
Multisync::sync() {

  Synchronized autolocker(_co);

  _count++;

  if (debug)
    cerr << "waiting in Multisync, ID(count): " << this << "(" << _count <<")" << endl;

  if (_count < _n)
    _co.wait();
  else {
    _count = 0;
    _co.broadcast();
  }
}

void
Multisync::init() {

  _count = 0;
}

void
Multisync::sync(string& msg) {

  Synchronized autolocker(_co);

  _count++;

  if (debug)
    cerr << "waiting in Multisync, ID(count): " << msg << "(" << _count <<")" << endl;

  if (_count < _n)
    _co.wait();
  else {
    _count = 0;
    _co.broadcast();
  }


  cout << "           sync: " << msg << endl;
}

