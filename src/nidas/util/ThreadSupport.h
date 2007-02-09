//
//              Copyright 2004 (C) by UCAR
//

#ifndef NIDAS_UTIL_THREADSUPPORT_H
#define NIDAS_UTIL_THREADSUPPORT_H

#include <pthread.h>
#include <semaphore.h>
#include <string>

#include <nidas/util/Exception.h>

namespace nidas { namespace util {

/**
 * A C++ wrapper for a POSIX mutex.
 */
class Mutex 
{

public:
  static bool Debug;

  /**
   * Construct a "fast" POSIX mutex, giving it a name for
   * debugging.  See man page for pthread_mutex_init.
   */
  Mutex(const std::string& name);

  /**
   * Construct a "fast" POSIX mutex.
   * See man page for pthread_mutex_init.
   */
  Mutex();

  /**
   * Copy constructor. Creates a new, unlocked mutex.
   */
  Mutex(const Mutex& x);

  /** 
   * Destruct a Mutex. See man page for pthread_mutex_destroy.
   */
  ~Mutex() throw(Exception);

  /**
   * Lock the Mutex.
   */
  inline void lock() 
  {
      /*
       * pthread_mutex_lock only returns EINVAL if mutex has not
       * been properly initialized, or EDEADLK for "error checking" mutexes
       * This is a fast mutex, so it won't see EDEADLK.
       * Since the mutex must have been initialized in the constructor,
       * we'll ignore error values, and not throw an exception.
       */
      ::pthread_mutex_lock(&p_mutex);
  }

  /**
   * Unlock the Mutex.
   */
  inline void unlock()
  {
    /*
     * pthread_mutex_unlock only returns EINVAL if mutex has not
     * been properly initialized, or EPERM for "error checking" mutexes.
     * This is a fast mutex, so it won't see EPERM.
     * Since the mutex must have been initialized in the constructor,
     * we'll ignore error values, and not throw an exception.
     */
    ::pthread_mutex_unlock(&p_mutex);
  }

  /**
   * Get the pointer to the pthread_mutex_t.
   */
  pthread_mutex_t* ptr();

  /**
   * Give the mutex a name, for diagnostic purposes.
   **/
  void setName(const std::string& nm);

  /**
   * Return the name of this mutex.
   */
  const std::string &getName() const
  {
    return name;
  }

private:
  /**
   * No assignment allowed.
   */
  Mutex& operator=(const Mutex&);

  pthread_mutex_t p_mutex;
  std::string name;
};

/**
 * A wrapper class for a Posix condition variable
 **/
class Cond 
{

public:

  /**
   * Construct a POSIX condition variable, with default attributes,
   * and give it a name for debugging.  See man page for pthread_cond_init.
   */
  Cond(const std::string& name);

  /** 
   * Construct a POSIX condition variable, with default attributes.
   * See man page for pthread_cond_init.
   */
  Cond();

  /**
   * Copy constructor. Creates new unlocked condition variable and mutex.
   */
  Cond (const Cond &x);

  /** 
   * Destruct a Cond. See man page for pthread_cond_destroy.
   */
  ~Cond() throw(Exception);

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
  inline void lock()
  {
    mutex.lock();
  }

  /**
   * Unlock the mutex associated with the condition variable.
   * @see lock() for an example.
   */
  inline void unlock()
  {
    mutex.unlock();
  }

  /**
   * Unblock at least one thread waiting on the condition variable.
   * @see lock() for an example.  According to the man page, it is
   * not safe to call Cond::signal() from an asynchronous signal
   * handler.
   */
  inline void signal()
  {
    ::pthread_cond_signal (&p_cond);	// never returns error code
  }

  /**
   * Restart all threads waiting on the condition variable.
   * @see lock().
   */
  inline void broadcast()
  {
    ::pthread_cond_broadcast (&p_cond);	// never returns error code
  }

  /**
   * Wait on the condition variable.
   * @see lock() for an example.
   * The cond_wait() call does several things:
   *   1. It immediately unlocks the mutex 
   *   2. It blocks until the condition variable is signalled
   *   3. It locks the mutex again
   */
  inline void Cond::wait()
  {
    ::pthread_cond_wait (&p_cond, mutex.ptr());	// never returns error
  }

private:
  /**
   * No assignment allowed.
   */
  Cond &operator= (const Cond &);
  pthread_cond_t p_cond;
  Mutex mutex;
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
    Semaphore()
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
  Synchronized (Cond &cond_) : mutexp(0),condp(&cond_)
  {
    condp->lock();
  }
  Synchronized (Mutex &mutex_) : mutexp(&mutex_),condp(0)
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
  Autolock (Cond &cond_) : mutexp(0),condp(&cond_)
  {
    cond_.lock();
  }
  /**
   * Construct the guard object and lock() the lock.
   **/
  Autolock (Mutex &mutex_) : mutexp(&mutex_),condp(0)
  {
    mutex_.lock();
  }

  /**
   * On destruction, unlock the lock.
   **/
  ~Autolock ()
  {
    if (condp) condp->unlock();
    else if (mutexp) mutexp->unlock();
  }

private:
  Mutex *mutexp;
  Cond *condp;

private:
  Autolock (const Autolock &);
  Autolock &operator= (const Autolock &);
    
};

}}	// namespace nidas namespace util

#endif

