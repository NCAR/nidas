//
//              Copyright 2004 (C) by UCAR
//

#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <sstream>

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/IOException.h>

using namespace std;
using namespace nidas::util;

MutexAttributes::MutexAttributes()
{
    ::pthread_mutexattr_init(&_attrs);
}

MutexAttributes::MutexAttributes(const MutexAttributes& x)
{
    // could we do _attrs = x_attrs; ?
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

RWLockAttributes::RWLockAttributes()
{
    ::pthread_rwlockattr_init(&_attrs);
}

RWLockAttributes::RWLockAttributes(const RWLockAttributes& x)
{
    // could we do _attrs = x_attrs; ?
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

Mutex::Mutex(int type) throw()
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
    ::pthread_mutex_init (&p_mutex,_attrs.ptr());
}

Mutex::Mutex(const MutexAttributes& attrs) throw(Exception) : _attrs(attrs)
{
    if (::pthread_mutex_init (&p_mutex,_attrs.ptr()))
        throw Exception("Mutex(attrs) constructor",errno);
}

/*
 * Copy constructor. Creates a new, unlocked mutex.
 */
Mutex::Mutex(const Mutex& x) throw() :_attrs(x._attrs)
{
    ::pthread_mutex_init (&p_mutex,_attrs.ptr());
}

Mutex::~Mutex() throw(Exception)
{
    int ret = 0;
    if ((ret = ::pthread_mutex_destroy(&p_mutex))) {
        switch(ret) {
        case EBUSY:
// If you're getting terminate messages with Exception "~Mutex", then #define this
// and run in valgrind in order to figure out where it is happening.
#ifdef DO_SEGFAULT_FOR_MUTEX_DEBUGGING
            {
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
     return &p_mutex;
}


Cond::Cond() throw() : mutex() 
{
    /* Can fail:
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * EINVAL: invalid attributes (we're  not passing attributer)
     * EBUSY: reinitialize an object already initialized (shouldn't happen)
     * We won't throw an exception here, and wait to throw it
     * when one tries a lock.
     */
    ::pthread_cond_init (&p_cond, 0);       // default attributes
}

Cond::Cond(const Cond& x) throw() : mutex(x.mutex)
{
    ::pthread_cond_init (&p_cond, 0);
}

Cond::~Cond() throw(Exception)
{
    if (::pthread_cond_destroy (&p_cond) && errno != EINTR) {
        switch(errno) {
        case EBUSY:
            throw Exception("~Cond",
                "Cond is being waited on by another thread");
        default:
            throw Exception("~Cond",errno);
        }
    }
}

RWLock::RWLock() throw()
{
    /* Can fail:
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * EPERM: insufficient privilege (not sure what this is about)
     * EBUSY: reinitialize an object already initialized (shouldn't happen)
     * We won't throw an exception here, and wait to throw it
     * when one tries a lock.
     */
    ::pthread_rwlock_init (&p_rwlock, _attrs.ptr());
}

RWLock::RWLock(const RWLockAttributes& attrs) throw(Exception) : _attrs(attrs)
{
    if (::pthread_rwlock_init (&p_rwlock,_attrs.ptr()))
        throw Exception("RWLock(attrs) constructor",errno);
}

/*
 * Copy constructor. Creates a new, unlocked rwlock.
 */
RWLock::RWLock(const RWLock& x) throw() : _attrs(x._attrs)
{
      ::pthread_rwlock_init (&p_rwlock, _attrs.ptr());
}

RWLock::~RWLock() throw(Exception)
{
    if (::pthread_rwlock_destroy(&p_rwlock)) {
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
     return &p_rwlock;
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

