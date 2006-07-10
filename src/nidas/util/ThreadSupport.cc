//
//              Copyright 2004 (C) by UCAR
//

#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <iostream>

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>

using namespace std;
using namespace nidas::util;

/**
 * A fast pthread mutex.
 */
Mutex::Mutex(const std::string& n) : name(n)
{
// mutex attribute of 0 gives a default
// type of "fast". See the man page on pthread_mutex
//	pthread_mutexattr_t attr;
//	pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
  ::pthread_mutex_init (&p_mutex, 0);		// always returns 0
}

Mutex::Mutex() : name("Mutex")
{
  ::pthread_mutex_init (&p_mutex, 0);		// always returns 0
}

/*
 * Copy constructor. Creates a new, unlocked mutex.
 */
Mutex::Mutex(const Mutex& x): name(x.name)
{
  ::pthread_mutex_init (&p_mutex, 0);		// always returns 0
}

Mutex::~Mutex() throw(Exception) {
  /* In LinuxThreads no resources are associated with mutex objects.
   * pthread_mutex_destroy only checks that the mutex is unlocked.
   */
  if (::pthread_mutex_destroy(&p_mutex) && errno != EINTR)
    throw Exception(string("~") + getName() + ": " + Exception::errnoToString(errno));
}


void
Mutex::lock()
{
  // Have to be careful with logging here, since the log appender may call
  // Thread::currentThread() or Thread::currentName(), and so we can't call
  // them here or we risk a recursive lock and consequent deadlock.  For
  // that matter, I can't get logging to work here at all at present without
  // a core dump, so leave it out.

  /*
   * pthread_mutex_lock only returns EINVAL if mutex has not
   * been properly initialized, or EDEADLK for "error checking" mutexes
   * This is a fast mutex, so it won't see EDEADLK.
   * Since the mutex must have been initialized in the constructor,
   * we'll ignore error values, and not throw an exception.
   */
  ::pthread_mutex_lock(&p_mutex);
}

void
Mutex::unlock()
{
  // See the note about logging in lock() above.

  /*
   * pthread_mutex_unlock only returns EINVAL if mutex has not
   * been properly initialized, or EPERM for "error checking" mutexes.
   * This is a fast mutex, so it won't see EPERM.
   * Since the mutex must have been initialized in the constructor,
   * we'll ignore error values, and not throw an exception.
   */
  ::pthread_mutex_unlock(&p_mutex);
}


pthread_mutex_t*
Mutex::ptr() {
   return &p_mutex;
 }

/** give the mutex a name, for diagnostic purposes
 */
void Mutex::setName(const string& nm)
{
	name = nm;
}

Cond::Cond(const std::string& name_) : mutex(name_) 
{
  ::pthread_cond_init (&p_cond, 0);	// never returns an error code
}

Cond::Cond() : mutex("Cond") 
{
  // there are no attributes for the cond_init
  ::pthread_cond_init (&p_cond, 0);	// never returns error code
}

Cond::Cond(const Cond& x) : mutex(x.mutex)
{
  // there are no attributes for the cond_init
  ::pthread_cond_init (&p_cond, 0);	// never returns error code
}

Cond::~Cond() throw(Exception)
{
  /* In LinuxThreads no resources are associated with condition variables.
   * pthread_cond_destroy only checks that no threads are waiting
   *  on the condition variable.
   */

  if (::pthread_cond_destroy (&p_cond) && errno != EINTR)
    throw Exception(string("~") + mutex.getName() + ": " + Exception::errnoToString(errno));
}


void
Cond::signal() {
  ::pthread_cond_signal (&p_cond);	// never returns error code
}


void
Cond::broadcast() {
  ::pthread_cond_broadcast (&p_cond);	// never returns error code
}

void Cond::wait()
{
//
// A condition variable is sort of obscure in the way that it works!
// A little explanation helps:
// The cond_wait() call does several things:
//   1. It immediately unlocks the mutex 
//   2. It blocks until the condition variable is signalled
//   3. It locks the mutex again
//
  // Thread *thread = Thread::currentThread ();
  ::pthread_cond_wait (&p_cond, mutex.ptr());	// never returns error
}

Multisync::Multisync(int n):
 _co("Multisync"), _n(n), _count(0), debug(0) {

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

