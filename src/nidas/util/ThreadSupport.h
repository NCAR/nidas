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

#ifndef NIDAS_UTIL_THREADSUPPORT_H
#define NIDAS_UTIL_THREADSUPPORT_H

#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <iostream>

#include "Exception.h"

namespace nidas { namespace util {

/**
 * A C++ wrapper for a POSIX mutex attributes. These are
 * customizations for Mutexes, and are only needed
 * if you want more than the default error checking, if you want
 * recursive behaviour where a thread can hold more than lock
 * on the same mutex, if you are using fine-grained thread
 * scheduling priority, or want to share a Mutex between processes.
 *
 * See man pages for pthread_mutexattr_init, pthread_mutexattr_settype,
 * pthread_mutexattr_setprioceiling, pthread_mutexattr_setprotocol,
 * and pthread_mutexattr_setpshared.
 */
class MutexAttributes
{
public:

    /**
     * Create instance with default values, type = PTHREAD_MUTEX_DEFAULT,
     * priority protocol=PTHREAD_PRIO_NONE and
     * pshared = PTHREAD_PROCESS_PRIVATE.
     */
    MutexAttributes();

    /**
     * Copy constructor.
     */
    MutexAttributes(const MutexAttributes&);

    ~MutexAttributes();

    /**
     * Set the mutex type attribute, one of
     * PTHREAD_MUTEX_NORMAL,PTHREAD_MUTEX_ERRORCHECK,PTHREAD_MUTEX_RECURSIVE,
     * or PTHREAD_MUTEX_DEFAULT. See pthread_mutexattr_settype.
     */
    void setType(int val) throw(Exception);
    int getType() const;

#ifdef PTHREAD_PRIO_INHERIT
    /**
     * Set the maximum priority level of this mutex.
     * See pthread_mutexattr_setprioceiling.
     * Set the priority ceiling higher or equal to the
     * highest priority of all threads that may lock the mutex.
     * Should be within the range of priorities defined under
     * the SCHED_FIFO scheduling priority.
     */
    void setPriorityCeiling(int val) throw(Exception);
    int getPriorityCeiling() const;
#endif

#ifdef PTHREAD_PRIO_INHERIT
    /**
     * Set the mutex protocol attribute, one of
     * PTHREAD_PRIO_NONE,PTHREAD_PRIO_INHERIT,PTHREAD_PRIO_PROTECT.
     * See pthread_mutexattr_setprotocol.
     */
    void setProtocol(int val) throw(Exception);
    int getProtocol() const;
#endif

    /**
     * Set the mutex pshared attribute, one of
     * PTHREAD_PROCESS_PRIVATE or PTHREAD_PROCESS_SHARED.
     * See pthread_mutexattr_setpshared.
     */
    void setPShared(int val) throw(Exception);
    int getPShared() const;


    pthread_mutexattr_t* ptr()
    {
        return &_attrs;
    }
private:
    pthread_mutexattr_t _attrs;
};

/**
 * A C++ wrapper for a POSIX rwlock attributes.
 * The only supported customization is to be able to
 * share a RWLock between processes.
 */
class RWLockAttributes
{
public:
    RWLockAttributes();

    /**
     * Copy constructor.
     */
    RWLockAttributes(const RWLockAttributes&);

    ~RWLockAttributes();

    /**
     * Set the mutex pshared attribute, one of
     * PTHREAD_PROCESS_PRIVATE or PTHREAD_PROCESS_SHARED.
     * See pthread_rwlockattr_setpshared.
     */
    void setPShared(int val) throw(Exception);
    int getPShared() const;


    pthread_rwlockattr_t* ptr()
    {
        return &_attrs;
    }
private:
    pthread_rwlockattr_t _attrs;
};

/**
 * A C++ wrapper for a POSIX mutex.
 */
class Mutex 
{

public:
    /**
     * Construct a POSIX mutex of the given type.
     * @param type One of PTHREAD_MUTEX_NORMAL,
     *  PTHREAD_MUTEX_ERRORCHECK, PTHREAD_MUTEX_RECURSIVE or
     *  PTHREAD_MUTEX_DEFAULT (default). See man page for 
     *  pthread_mutex_init, and pthread_mutexattr_settype.
     * The following error conditions could occur in this
     * constructor, but they are not thrown as exceptions.
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * An exception will likely occur later when an lock attempt
     * is made on this Mutex.
     */
    explicit Mutex(int type=PTHREAD_MUTEX_DEFAULT) throw();

    /**
     * Construct a POSIX mutex with the given attributes.
     * This constructor can throw an exception in the following
     * circumstances:
     * EAGAIN: system lacked resources
     * ENOMEM: system lacked memory
     * EINVAL: invalid attributes
     */
    Mutex(const MutexAttributes& attr) throw(Exception);

    /**
     * Copy constructor. Creates a new, unlocked mutex with
     * the same attributes as x. The error handling is the same
     * as with the default constructor.
     */
    Mutex(const Mutex& x) throw();

    /** 
     * Destruct a Mutex. See man page for pthread_mutex_destroy.
     * Mutex must not be locked, otherwise std::terminate() is called.
     */
    ~Mutex();

    /**
     * Lock the Mutex.
     * An exception will be thrown with error condition
     * EDEADLK if this is a PTHREAD_MUTEX_ERRORCHECK type Mutex, and
     * the current thread has already locked it.
     * An exception for error EINVAL will be thrown if the Mutex was
     * created with the protocol attribute having the value
     * PTHREAD_PRIO_PROTECT and the  calling  thread’s priority
     * is higher than the mutex’s current priority ceiling.
     */
    void lock() throw(Exception)
    {
        int res;
        if((res = ::pthread_mutex_lock(&_p_mutex)))
            throw Exception("Mutex::lock",res);
    }

    /**
     * Unlock the Mutex. An exception will be thrown for error
     * condition EPERM if this is an PTHREAD_MUTEX_ERRORCHECK Mutex
     * and the current thread does not hold a lock.
     */
    void unlock() throw(Exception)
    {
        int res;
        if ((res = ::pthread_mutex_unlock(&_p_mutex)))
            throw Exception("Mutex::unlock",res);
    }

    /**
     * Get the pointer to the pthread_mutex_t.
     */
    pthread_mutex_t* ptr();

private:
    /**
     * No assignment allowed.
     */
    Mutex& operator=(const Mutex&);

    pthread_mutex_t _p_mutex;

    MutexAttributes _attrs;

};

/**
 * A wrapper class for a Posix condition variable
 **/
class Cond 
{

public:

    /** 
     * Construct a POSIX condition variable, with default attributes.
     * See man page for pthread_cond_init.
     */
    Cond() throw();

    /**
     * Copy constructor. Creates new unlocked condition variable and mutex.
     */
    Cond (const Cond &x) throw();

    /** 
     * Destruct a Cond. See man page for pthread_cond_destroy.
     * Cond must not be waited on by any thread, otherwise
     * std::terminate() is called.
     */
    ~Cond();

    /**
     * Lock the mutex associated with the condition variable.
     * Here is an example (lifted from the pthread_cond_init man page):
     * of two threads sharing variables x and y. One thread waits
     * for a signal that x is greater than y:
     * @code
     * int x,y;
     * Cond xyCond;
     * ...
     * xyCond.lock();
     * while (x <= y) xyCond.wait();
     * // operate on x and y
     * xyCond.unlock();
     * @endcode
     * Another thread manipulates x and y and signals when x > y:
     * @code
     * xyCond.lock();
     * // modify x and y
     * if (x > y) xyCond.signal();
     * xyCond.unlock();
     * @endcode
     */
    void lock() throw(Exception)
    {
        _mutex.lock();
    }

    /**
     * Unlock the mutex associated with the condition variable.
     * @see lock() for an example.
     */
    void unlock() throw(Exception)
    {
        _mutex.unlock();
    }

    /**
     * Unblock at least one thread waiting on the condition variable.
     * @see lock() for an example.  According to the man page, it is
     * not safe to call Cond::signal() from an asynchronous signal
     * handler.
     */
    void signal() throw(Exception)
    {
        // only fails with EINVAL if _p_cond is not initialized.
        int res;
        if ((res = ::pthread_cond_signal (&_p_cond)))
            throw Exception("Cond::signal",res);
    }

    /**
     * Restart all threads waiting on the condition variable.
     * @see lock().
     */
    void broadcast()
    {
        int res;
        // only fails with EINVAL if _p_cond is not initialized.
        if ((res = ::pthread_cond_broadcast (&_p_cond)))
            throw Exception("Cond::broadcast",res);
    }

    /**
     * Wait on the condition variable.
     * @see lock() for an example.
     * wait() does several things:
     *   1. It immediately unlocks the mutex 
     *   2. It blocks until the condition variable is signalled
     *   3. It locks the mutex again
     * wait() is a cancellation point, which sets up the
     * appropriate cancellation cleanup handlers so that the
     * mutex is unlocked if the thread is cancelled.
     */
    void wait() throw(Exception);

private:
    /**
     * No assignment allowed.
     */
    Cond &operator= (const Cond &);

    pthread_cond_t _p_cond;

    Mutex _mutex;

};

/**
 * A C++ wrapper for a POSIX rwlock.
 */
class RWLock 
{

public:

    /**
     * Construct a POSIX rwlock.
     * See man page for pthread_rwlock_init.
     */
    RWLock() throw();

    RWLock(const RWLockAttributes& attr) throw(Exception);
    /**
     * Copy constructor. Creates a new, unlocked rwlock.
     */
    RWLock(const RWLock& x) throw();

    /** 
     * Destruct a RWLock. See man page for pthread_rwlock_destroy.
     * RwLock must not be locked, otherwise std::terminate() is called.
     */
    ~RWLock();

    /**
     * Acquire a read lock. May throw an exception if the maximum
     * number of read locks for this RWLock has been exceeded.
     */
    void rdlock() throw(Exception)
    {
        int res;
        if ((res = ::pthread_rwlock_rdlock(&_p_rwlock)))
            throw Exception("RWLock::rdlock",res);
    }

    /**
     * Acquire a write lock. May throw an exception if the current
     * thread already owns the lock.
     */
    void wrlock() throw(Exception)
    {
        int res;
        if ((res = ::pthread_rwlock_wrlock(&_p_rwlock)))
            throw Exception("RWLock::wrlock",res);
    }

    /**
     * Unlock the RWLock. Will throw an exception EPERM
     * if the current thread does not hold a lock.
     */
    void unlock() throw(Exception)
    {
        int res;
        if ((res = ::pthread_rwlock_unlock(&_p_rwlock)))
            throw Exception("RWLock::unlock",res);
    }

    /**
     * Get the pointer to the pthread_rwlock_t.
     */
    pthread_rwlock_t* ptr();

private:
    /**
     * No assignment allowed.
     */
    RWLock& operator=(const RWLock&);

    pthread_rwlock_t _p_rwlock;

    RWLockAttributes _attrs;

};

/**
 * A POSIX semaphore.
 */
class Semaphore 
{
public:

    /**
     * Constructor.
     */
    Semaphore():_sem()
    {
	sem_init(&_sem,0,0);
    }


    /** 
     * Destructor.
     */
    ~Semaphore()
    {
	sem_destroy(&_sem);
    }

    /** 
     * Suspend calling thread until the semaphore has a non-zero count.
     * Then atomically decrement the semaphore count and return;
     */
    void wait() throw()
    {
        sem_wait(&_sem);
    }

    /**
     * Do a non-blocking wait on the Semaphore.
     * @return true Semaphore had a non-zero count, which was decremented.
     *         false Semaphore was zero. Not changed.
     */
    bool check() throw()
    {
        return sem_trywait(&_sem) == 0;
    }

    /**
     * Atomically increment the Semaphore.
     */
    void post() throw()
    {
	// man page for sem_post:
	// return value of sem_post can be < 0, in which case errno is ERANGE:
	// the  semaphore value would exceed SEM_VALUE_MAX
	// 	(the semaphore count is left  unchanged  in this case)
	// Hmmm: do we throw an exception for this pathelogical situation,
	// which compels users to have to catch it?  Naa.
        sem_post(&_sem);
    }

    /**
     * Get the current value of the Semaphore.
     */
    int getValue() throw()
    {
	int val;
        sem_getvalue(&_sem,&val);
	return val;
    }

    /**
     * Get the pointer to the semaphore (for legacy C code).
     */
    sem_t* ptr() { return &_sem; }

private:

    /**
     * No copying.
     */
    Semaphore(const Semaphore&);

    /**
     * No assignment allowed.
     */
    Semaphore& operator=(const Semaphore&);

    sem_t _sem;
};

class Multisync
{

public:
  Multisync(int n);
  void sync();
  void sync(std::string& msg);
  void init();   // to reinitialize

private:
  Cond  _co;
  int _n;
  int _count;
  int debug;

};


/**
 * Synchronized is used a simple guard object for critical sections.  Upon
 * construction it lock()s the given Lock, and then when it goes out of
 * scope, either through a normal exit or an exception, it will be
 * destroyed and the Lock will be unlock()ed.  Inside the critical section
 * the thread can call wait() on the Synchronized object to wait on the
 * Lock condition.
 **/
class Synchronized
{
public:

    /**
     * Construct the guard object and lock() the lock.  As for Lock, be wary
     * of recursively entering sections Synchronized on the same lock within
     * the same thread.  The default, posix mutexes are not recursive and so
     * the thread will deadlock.
     **/
    Synchronized (Cond &cond_) throw(Exception): mutexp(0),condp(&cond_)
    {
        condp->lock();
    }
    Synchronized (Mutex &mutex_) throw(Exception): mutexp(&mutex_),condp(0) 
    {
        mutexp->lock();
    }

    /**
     * On destruction, unlock the lock.
     **/
    ~Synchronized ()
    {
        if (condp) condp->unlock();
        else if (mutexp) mutexp->unlock();
    }

private:
    Mutex *mutexp;
    Cond *condp;

private:
    Synchronized (const Synchronized &);
    Synchronized &operator= (const Synchronized &);
    
};

/**
 * Autolock is used a simple guard object for critical sections.  Upon
 * construction it lock()s the given Lock, and then when it goes out of
 * scope, either through a normal exit or an exception, it will be
 * destroyed and the Lock will be unlock()ed.  Inside the critical section
 * the thread can call wait() on the Synchronized object to wait on the
 * Lock condition.
 **/
class Autolock
{
public:

    /**
     * Construct the guard object and lock() the lock.
     **/
    Autolock (Cond &cond) throw(Exception): _mutexp(0),_condp(&cond)
    {
        cond.lock();
    }
    /**
     * Construct the guard object and lock() the lock.
     **/
    Autolock (Mutex &mutex) throw(Exception): _mutexp(&mutex),_condp(0)
    {
        mutex.lock();
    }

    /**
     * On destruction, unlock the lock.
     **/
    ~Autolock ()
    {
        if (_condp) _condp->unlock();
        else if (_mutexp) _mutexp->unlock();
    }

  private:
      Mutex *_mutexp;
      Cond *_condp;

  private:
      Autolock (const Autolock &);
      Autolock &operator= (const Autolock &);
    
};

/**
 * Autolock for acquiring/releasing a read lock on a RWLock.
 **/
class AutoRdLock
{
public:

    /**
     * Construct the guard object and lock() the lock.
     **/
    AutoRdLock (RWLock &rwlock) throw(Exception): _rwlock(rwlock)
    {
        _rwlock.rdlock();
    }

    /**
     * On destruction, unlock the lock.
     **/
    ~AutoRdLock ()
    {
        _rwlock.unlock();
    }

private:
    RWLock& _rwlock;

private:
    AutoRdLock (const Autolock &);
    AutoRdLock &operator= (const Autolock &);
};

/**
 * Autolock for acquiring/releasing a write lock on a RWLock.
 **/
class AutoWrLock
{
public:

    /**
     * Construct the guard object and lock() the lock.
     **/
    AutoWrLock (RWLock &rwlock) throw(Exception): _rwlock(rwlock)
    {
        _rwlock.wrlock();
    }

    /**
     * On destruction, unlock the lock.
     **/
    ~AutoWrLock ()
    {
        _rwlock.unlock();
    }

private:
    RWLock& _rwlock;

private:
    AutoWrLock (const Autolock &);
    AutoWrLock &operator= (const Autolock &);
};


}}	// namespace nidas namespace util

#endif

