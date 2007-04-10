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

Mutex::Mutex()
{
  ::pthread_mutex_init (&p_mutex, 0);		// always returns 0
}

/*
 * Copy constructor. Creates a new, unlocked mutex.
 */
Mutex::Mutex(const Mutex& x)
{
  ::pthread_mutex_init (&p_mutex, 0);		// always returns 0
}

Mutex::~Mutex() throw(Exception) {
  /* In LinuxThreads no resources are associated with mutex objects.
   * pthread_mutex_destroy only checks that the mutex is unlocked.
   */
  if (::pthread_mutex_destroy(&p_mutex) && errno != EINTR)
    throw Exception(string("~Mutex") + ": " + Exception::errnoToString(errno));
}

pthread_mutex_t*
Mutex::ptr() {
   return &p_mutex;
}


Cond::Cond() : mutex() 
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
    throw Exception(string("~Cond") + ": " + Exception::errnoToString(errno));
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

