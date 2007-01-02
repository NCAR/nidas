//
//              Copyright 2004 (C) by UCAR
//

/* needed with gcc 2.X and strsignal */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>

#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <pthread.h>
#include <cstring>
#include <csignal>

#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace nidas::util;


Mutex Thread::_threadsMutex;
map<pthread_t,Thread*,less<pthread_t> > Thread::_threads;

/**
 * the signals that we handle because the user has called unblockSignal()
 * on a thread.
 */
set<int> Thread::_handledSignals;

void
Thread::sigAction(int sig,siginfo_t* siginfo,void* ptr)
{
  pthread_t id = Thread::currentThreadId();
  Thread *thrptr = Thread::currentThread();

  if (thrptr) {
    cerr << "thread " << thrptr->getName() << 
    	" received signal " << strsignal(sig) << "(" << sig << ")" << 
	" si_signo=" << siginfo->si_signo << 
	" si_errno=" << siginfo->si_errno << 
	" si_code=" << siginfo->si_code << endl;
    thrptr->signalHandler(sig,siginfo);
  }
  else {
    cerr << "unknown thread " << "(" << id << ")" <<
    	" received signal " << strsignal(sig) << "(" << sig << ")" << 
	" si_signo=" << siginfo->si_signo << 
	" si_errno=" << siginfo->si_errno << 
	" si_code=" << siginfo->si_code << endl;
  }
}

/*static*/
pthread_t
Thread::currentThreadId ()
{
  return ::pthread_self();
}


/*static*/
Thread *
Thread::currentThread()
{
  pthread_t id = currentThreadId();
  Thread *thrptr = 0;

  Synchronized sync(_threadsMutex);
  const threadmap_t::iterator& it = _threads.find(id);
  if (it != _threads.end()) thrptr = it->second;
  return thrptr;
}

/*static*/
std::string Thread::unknownName = std::string("unknown");


void
Thread::registerThread()
{
  Synchronized sync(_threadsMutex);
  _threads[_id] = this;
}

void
Thread::unregisterThread()
{
  Synchronized sync(_threadsMutex);
  const threadmap_t::iterator& it = _threads.find(_id);
  if (it != _threads.end()) _threads.erase(it);
}

Thread::Thread(const std::string& name, bool detached) : 
    _mutex(name),
    _joinMutex(name),
    _fullnamemutex(name),
    _name(name),
    _fullname(name),
    _id(0),
    _interrupted(false),
    _cancel_enabled(true),
    _cancel_deferred(true),
    _running(false),
    _needsjoining(false),
    _exception(0),
    _detached(detached)
{
  ::pthread_attr_init(&thread_attr);
  ::pthread_attr_setdetachstate(&thread_attr,
      _detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
  sigemptyset(&unblockedSignals);
  sigemptyset(&blockedSignals);

  // get current set of signals
  // sigprocmask(SIG_BLOCK,&unblockedSignals,&blockedSignals);
}


/* Copy constructor */
Thread::Thread(const Thread& x):
    _mutex(x._mutex),
    _joinMutex(x._joinMutex),
    _fullnamemutex(x._fullnamemutex),
    _name(x._name),
    _fullname(x._fullname),
    _id(0),
    _interrupted(false),
    _cancel_enabled(x._cancel_enabled),
    _cancel_deferred(x._cancel_deferred),
    _running(false),
    _needsjoining(false),
    _exception(0),
    _detached(x._detached)
{
  ::pthread_attr_init(&thread_attr);
  ::pthread_attr_setdetachstate(&thread_attr,
      _detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
  unblockedSignals = x.unblockedSignals;
  blockedSignals = x.blockedSignals;
}

Thread::~Thread()
{
  if (_exception) delete _exception;
  ::pthread_attr_destroy(&thread_attr);

  if (_running) {
    Exception e(string("thread ") + getName() +
  	" still running at destruction time");
    cerr << e.what() << endl;
    // throw e;
  }
  if (!_detached && _needsjoining) {
    Exception e(string("thread ") + getName() +
  	" not joined at destruction time");
    cerr << e.what() << endl;
    // throw e;
  }
}

void Thread::blockSignal(int sig) {
  // set the signal masks which are then
  // applied to the thread at the beginning of the run method.
  sigaddset(&blockedSignals,sig);
  sigdelset(&unblockedSignals,sig);


  // pthread_sigmask changes the signal mask of the current thread.
  // so we check that the current thread is this thread.
  if (_running && Thread::currentThread() == this)
    ::pthread_sigmask(SIG_BLOCK,&blockedSignals,(sigset_t*)0);
}

void Thread::unblockSignal(int sig) {
  sigaddset(&unblockedSignals,sig);
  sigdelset(&blockedSignals,sig);
  thr_add_sig(sig);
  if (_running && Thread::currentThread() == this) {
      ::pthread_sigmask(SIG_UNBLOCK,&unblockedSignals,(sigset_t*)0);
    }
}

/* static */
void Thread::thr_add_sig(int sig)
{
  // signal handlers are shared between all threads (and main)
  _threadsMutex.lock();
  if (_handledSignals.find(sig) == _handledSignals.end()) {
      struct sigaction act;
      sigset_t set;
      sigemptyset(&set);
      act.sa_mask = set;
      act.sa_flags = SA_SIGINFO;
      act.sa_sigaction = sigAction;
      ::sigaction(sig,&act,(struct sigaction *)0);
      _handledSignals.insert(sig);
  }
  _threadsMutex.unlock();
}

/* static */
void
Thread::thr_delete(void *me) 
{ 
  Thread *thr = (Thread*)me;
#ifdef DEBUG
  cerr << "thr_delete of " << thr->getFullName() << endl;
#endif
  thr->unregisterThread();
  thr->_running = false;
  delete thr;
}

/* static */
void
Thread::thr_cleanup(void *me) 
{ 
    Thread *thr = (Thread*)me;
#ifdef DEBUG
    cerr << "thr_cleanup of " << thr->getFullName() << endl;
#endif
    thr->_running = false;
    thr->unregisterThread();
    thr->_joinMutex.unlock();
}

/* static */
void*
Thread::thr_run(void *me) 
{ 
  void *result = 0;
  Thread* thisThread = (Thread*) me;


  /* Warning: pthread_cleanup_push and pthread_cleanup_pop are macros.
   * pthread_cleanup_push expands to a start of a block of code: 
   * {
   *   ...
   * and pthread_clean_pop expands to the end of the block of code:
   *   ...
   * }
   */

  thisThread->_joinMutex.lock();

  /* thr_cleanup will be called whenever this thread quits */
  pthread_cleanup_push(thr_cleanup,me);

  // At this point we have entered the new thread, but we are in a static
  // method.  So pass the work back into our thread object instance so it
  // can access its own members and call its run method.
  //
  result = thisThread->pRun();

  // saw strange behavior (arm-linux-g++ 3.4.2, on viper)
  // If one does a join and then immediate delete of a thread, 
  // the Thread destructor complained that _running was still true.
  //
  // thr_cleanup sets _running to false, so pthread_join
  // was finishing and the Thread destructor entered before the
  // pthread_cleanup_pop function was finished. Hmmm.
  //
  // I think this means that the pthread_cleanup functions are
  // independent of this thr_run function - they can finish
  // after thr_run returns, assuming that pthread_join
  // is not done until thr_run finishes.
  //
  // That means that the Thread pointer may not be valid within
  // thr_cleanup. This limits the usefulness of pthread_cleanup_push.
  //
  // This is the reason for _joinMutex:
  // 1. lock _joinMutex before the pthread_cleanup_push
  // 2. unlock it in thr_cleanup.
  // 3. lock it in join

  pthread_cleanup_pop(1);
  return result;
}

/* static */
void*
Thread::thr_run_detached(void *me) 
{ 
  void *result = 0;
  Thread *thisThread = (Thread *)me;

  // At this point we have entered the new thread, but we are in a static
  // method.  So pass the work back into our thread object instance so it
  // can access its own members and call its run method.
  //

  // If the thread is cancelled, it will not exit its run method
  // cleanly. This will catch that situation and delete the object.

  pthread_cleanup_push(thr_delete,thisThread);

  result = thisThread->pRun();

  pthread_cleanup_pop(1);

  return result;
}


void*
Thread::pRun()
{

    /*
     * The behavior of thread canceling and exceptions changed
     * with libstdc++.so.6.0.3 (Fedora Core 3).
     *
     * Prior to that, pRun did a catch of
     * Exception, std::exception and a catch(...) when it try-ed
     * the Thread::run method.
     * Also, pRun was declared to throw no exceptions
     *		void* pRun() throw()
     *
     * Doing a Thread::cancel in a program linked against
     * libstdc++.so.6.0.3 resulted in process termination and message
     *	"FATAL: exception not rethrown".
     *
     * Removing the extra catches, so that only Exception was
     * caught, but keeping the throw() declaration of pRun,
     * would still terminate with:
     * 	"terminate called without an active exception".
     *
     * Finally, removing the throw() declaration on pRun solved
     * the terminations.
     */

  /* wait until ::start has finished, so that the id is initialized. */

#ifdef SUPPORT_ASYNC_CANCEL
  pthread_cleanup_push_defer_np(
  	(void(*)(void*))::pthread_mutex_unlock,(void*)_mutex.ptr());
  _mutex.lock();
  pthread_cleanup_pop_restore_np(1);
#else
   /*
   * since mutex.lock is not a cancelation point, this
   * cannot be a victim of a deferred cancel.
   */
  // by syncing on _mutex here we know the ::start method is finished.
  _mutex.lock();
  _mutex.unlock();
#endif

  if (_exception) delete _exception;
  _exception = 0;

  ::pthread_sigmask(SIG_UNBLOCK,&unblockedSignals,(sigset_t*)0);
  ::pthread_sigmask(SIG_BLOCK,&blockedSignals,(sigset_t*)0);

  std::cerr << getFullName() << " run..." << std::endl;

  int result = RUN_EXCEPTION;

  try
  {
    result = this->run();
    std::cerr << getFullName() << " run method finished" << std::endl;
  }
  catch (const Exception& ex)
  {
    _exception = ex.clone();
    result = RUN_EXCEPTION;
    std::cerr << getFullName() << " run method exception:" <<
    	ex.toString() << std::endl;
  }

  return (void*)result;	// equivalent to calling pthread_exit(result);
}


void
Thread::start() throw(Exception)
{
  Synchronized sync(_mutex);
  if (! _running)
  {
    int status;

    /*
     * pthread_create can start the thr_run method before it sets the value of
     * the _id argument.  So the value of _id may
     * not be set if the run method needs it right away.
     * You must sync on _mutex in ::pRun to avoid this.
     */

    int state;
    ::pthread_attr_getdetachstate( &thread_attr, &state);

    if (state == PTHREAD_CREATE_DETACHED)
	status = ::pthread_create(&_id, &thread_attr, thr_run_detached, this);
    else
	status = ::pthread_create(&_id, &thread_attr, thr_run, this);

    if (status) 
      throw Exception(getName(),
      	string("pthread_create: ") + Exception::errnoToString(status));

    makeFullName();
    registerThread();

    // Make sure running is true before the calling thread resumes,
    _running = true;
    _needsjoining = true;
    _interrupted = false;
  }
}

/**
 * Perhaps there is an issue here. The caller cannot
 * tell the difference between an Exception thrown
 * because pthread_join returned an error, and an
 * Exception thrown by the threads run method.
 *
 * So when an Exception is thrown, either the thread cannot
 * be joined, or its run method threw an Exception.
 * Perhaps one doesn't really need to know.
 */
int
Thread::join() throw(Exception)
{
  // cerr << "doing Thread::join on " << getName() << endl;
  int status;
  void* thread_return = (void *)RUN_OK;
  /*
   * pthread_join will return EINVAL if thread is detached, ESRCH if
   * no thread with that ID found, EINVAL if another thread is
   * already waiting, or EDEADLK if I'm waiting on myself.
   */
  if (_id && (status = ::pthread_join(_id, &thread_return))) 
  {
    _id = 0;
    _needsjoining = false;
    throw Exception(getName(),
    	string("pthread_join: ") + Exception::errnoToString(status));
  }
  _joinMutex.lock();	// make sure thr_cleanup is done.
  _joinMutex.unlock();
  _id = 0;
  // _running = false;
  _needsjoining = false;
  // cerr << "Thread::joined on " << getName() << endl;
  if (thread_return == (void *) RUN_EXCEPTION && _exception)
  	throw *_exception;
  return (int) thread_return;
}

void
Thread::kill(int sig) throw(Exception)
{
  int status;
  if (_id && isRunning() &&
      !(status = ::pthread_kill(_id,0)) &&
      (status = ::pthread_kill(_id,sig)))
  {
    _id = 0;
    throw Exception(getName(),string("pthread_kill: ") + Exception::errnoToString(status));
  }
}

/*
 * Cancel this thread.
 */
void
Thread::cancel() throw(Exception)
{
  if (!_id) return;
  int status;
  if ((status = ::pthread_cancel(_id)))
    throw Exception(getName(),string("pthread_cancel: ") +
    	Exception::errnoToString(status));
}

void
Thread::setCancelEnabled(bool val)
{
  if (val != _cancel_enabled) {
      ::pthread_setcancelstate(
  	val ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE,0);
       _cancel_enabled = val;
  }
}

bool
Thread::isCancelEnabled() const { return _cancel_enabled; }

void
Thread::setCancelDeferred(bool val)
{
  if (val != _cancel_deferred) {
      ::pthread_setcanceltype(
      	val ? PTHREAD_CANCEL_DEFERRED : PTHREAD_CANCEL_ASYNCHRONOUS ,0);
      _cancel_deferred = val;
  }
}

bool
Thread::isCancelDeferred() const { return _cancel_deferred; }


const std::string&
Thread::getName() const throw()
{
  return _name;
}

void Thread::makeFullName() {

  std::ostringstream os;

  os << _name << "(id=" << _id;

  sched_param param;
  int policy = 0;

  if (::pthread_getschedparam(_id,&policy,&param) == 0) {
    switch (policy) {
    case SCHED_OTHER: os << ",Non-RT"; break;
    case SCHED_FIFO:  os << ",RT:FIFO"; break;
    case SCHED_RR:  os << ",RT:RR"; break;
    default:  os << ",RT:Unk"; break;
    }
  }
  os << ",prior=" << param.sched_priority;

  if (_detached) os << ",detached";
  else os << ",joinable";

  if (!_cancel_enabled) os << ",cancel=no";
  else {
    if (_cancel_deferred) os << ",cancel=deferred";
    else os << ",cancel=immed";
  }
  os << ")";

  /* Try to make sure someone doesn't get a half built name */
#ifdef SUPPORT_ASYNC_CANCEL
  pthread_cleanup_push_defer_np(
  	(void(*)(void*))pthread_mutex_unlock,(void*)_fullnamemutex.ptr());
#endif

  _fullnamemutex.lock();
  _fullname = os.str();

#ifdef SUPPORT_ASYNC_CANCEL
  pthread_cleanup_pop_restore_np(1);
#else
  _fullnamemutex.unlock();
#endif

}

const std::string
Thread::getFullName() throw()
{
  string val;

#ifdef SUPPORT_ASYNC_CANCEL
  pthread_cleanup_push_defer_np(
  	(void(*)(void*))pthread_mutex_unlock,(void*)_fullnamemutex.ptr());
#endif
  _fullnamemutex.lock();
  val = _fullname;

#ifdef SUPPORT_ASYNC_CANCEL
  pthread_cleanup_pop_restore_np(1);
#else
  _fullnamemutex.unlock();
#endif

  return val;
}


void
Thread::interrupt()
{
  _interrupted = true; 
}


bool Thread::setRealTimeRoundRobinPriority(int val) throw(Exception) {
  if (getuid() != 0) return false;
  setThreadScheduler(SCHED_RR,val);
  return true;
}

bool Thread::setRealTimeFIFOPriority(int val) throw(Exception) {
  if (getuid() != 0) return false;
  setThreadScheduler(SCHED_FIFO,val);
  return true;
}

bool Thread::setNonRealTimePriority() throw(Exception) {
  setThreadScheduler(SCHED_OTHER,0);
  return true;
}

void Thread::setThreadScheduler(int policy,int val) throw(Exception) {
  int status;
  sched_param param;
  memset(&param,0,sizeof(param));
  param.sched_priority = val;

  Synchronized autolock(_mutex);
  if (_running) {
    status = ::pthread_setschedparam(_id,policy,&param);
    if (status)
      throw Exception(getName(),
      	string("pthread_setschedparam:") + Exception::errnoToString(status));
  }
  else {
    status = ::pthread_attr_setschedpolicy(&thread_attr,policy);
    // int maxprior = sched_get_priority_max(policy);
    // cerr << "sched_get_priority_max(policy)=" << sched_get_priority_max(policy) << endl;
    if (status)
      throw Exception(getName(),
      	string("pthread_attr_setschedpolicy:") + Exception::errnoToString(status));
    status = ::pthread_attr_setschedparam(&thread_attr,&param);
    if (status)
      throw Exception(getName(),
      	string("pthread_setschedparam:") + Exception::errnoToString(status));
    status = ::pthread_attr_setinheritsched(&thread_attr,PTHREAD_EXPLICIT_SCHED);
    if (status)
      throw Exception(getName(),
      	string("pthread_setinheritsched:") + Exception::errnoToString(status));
  }
}
ThreadJoiner::ThreadJoiner(Thread* thrd):
	DetachedThread("ThreadJoiner"),thread(thrd)
{
}
ThreadJoiner::~ThreadJoiner()
{
}
int ThreadJoiner::run() throw() {
    struct timespec slp = { 2 , 0 };
    ::nanosleep(&slp,0);
    try {
	thread->join();
    }
    catch (const Exception& e) {
	cerr << thread->getName() << ": " << e.what() << endl;
    }
#ifdef DEBUG
    cerr << "joined " << thread->getName() << " deleting" << endl;
#endif
    delete thread;
    return RUN_OK;
}

/* static */
int Thread::test(int argc, char** argv)
{
    #if __GNUC__ >= 3
        std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
    #endif
    class ThreadTest1: public Thread {
    public:
	ThreadTest1(const std::string& name, bool detached) :
		Thread(name,detached)
	{
            blockSignal(SIGINT);
            blockSignal(SIGHUP);
            blockSignal(SIGTERM);
	}
	~ThreadTest1() { std::cerr << getName() << " dtor, running=" << isRunning() <<
		" joined=" << isJoined() << std::endl; }
	int run() throw(Exception)
	{
	    // std::cerr << getFullName() << " run starting" << std::endl;
	    struct timespec ts;
	    ts.tv_sec = 1;
	    ts.tv_nsec = 0;
	    int i;
	    for (i = 0; i < 4; i++) {
		cerr << getName().c_str() <<
			" checking amInterrupted, i=" << i << endl;
		if (amInterrupted()) break;
		std::cerr << getName() << " sleeping, i=" << i << std::endl;
		if (nanosleep(&ts,0) < 0) break;
	    }
	    std::cerr << getFullName() << " run finished" << std::endl;
	    return i;
	}
    };

    class ThreadTestD: public DetachedThread {
    public:
	ThreadTestD(const std::string& name) :
		DetachedThread(name) {}
	~ThreadTestD() { std::cerr << getName() << " dtor, running=" << isRunning() <<
		" joined=" << isJoined() << std::endl; }
	int run() throw(Exception)
	{
	    std::cerr << getFullName() << " run starting" << std::endl;
	    struct timespec ts;
	    ts.tv_sec = 1;
	    ts.tv_nsec = 0;
	    int i;
	    for (i = 0; i < 4; i++) {
		std::cerr << getName() << " checking amInterrupted, i=" << i << std::endl;
		if (amInterrupted()) break;
		std::cerr << getName() << " sleeping, i=" << i << std::endl;
		nanosleep(&ts,0);
	    }
	    std::cerr << getFullName() << " run finished" << std::endl;
	    return i;
	}
    };

    class ThreadTestE: public Thread {
    public:
	ThreadTestE(const std::string& name, bool detached) :
		Thread(name,detached) {}
	~ThreadTestE() { std::cerr << getName() << " dtor, running=" << isRunning() <<
		" joined=" << isJoined() << std::endl; }
	int run() throw(Exception)
	{
	    std::cerr << getFullName() << " run starting" << std::endl;
	    struct timespec ts;
	    ts.tv_sec = 1;
	    ts.tv_nsec = 0;
	    int i;
	    for (i = 0; i < 4; i++) {
		std::cerr << getName() << " checking amInterrupted, i=" << i << std::endl;
		if (amInterrupted()) break;
		std::cerr << getName() << " sleeping, i=" << i << std::endl;
		nanosleep(&ts,0);
		throw Exception("whacka-whacka");
	    }
	    std::cerr << getFullName() << " run finished" << std::endl;
	    return i;
	}
    };

    cerr << "***** starting t1 test" << endl;
    ThreadTest1 t1("test1",false);
    int jres;

    t1.start();
    jres = t1.join();
    std::cerr << t1.getFullName() << " join=" << jres << std::endl;
    if (jres != 4) {
	cerr << "Error: t1 test\n\n" << endl;
	return 1;
    }

    cerr << "***** finished t1 test\n\n" << endl;

    cerr << "***** starting t2 test" << endl;
    /* detached thread */
    ThreadTest1* t2 = new ThreadTest1("test2",true);
    t2->start();

    struct timespec ts;
    ts.tv_sec = 6;
    ts.tv_nsec = 0;
    nanosleep(&ts,0);
    cerr << "***** finished t2 test\n\n" << endl;

    try {
	cerr << "***** starting t3 test" << endl;
	/* cancel non-detached thread */
	ThreadTest1 t3("test3",false);
	t3.start();

	ts.tv_sec = 2;
	ts.tv_nsec = 0;
	nanosleep(&ts,0);
	t3.cancel();
	std::cerr << t3.getFullName() << " joining" << std::endl;
	jres = t3.join();
	std::cerr << t3.getFullName() << " join=" << jres << std::endl;
	if (jres != -1) {
	    cerr << "Error: t3 test\n\n" << endl;
	    return 1;
	}

	cerr << "***** finished t3 test\n\n" << endl;

	cerr << "***** starting second t3 test" << endl;
	// try starting again
	t3.start();
	ts.tv_sec = 2;
	ts.tv_nsec = 0;
	nanosleep(&ts,0);
	t3.cancel();
	// Don't join, the destructor should throw an exception
	// jres = t3.join();
	// std::cerr << t3.getFullName() << " join=" << jres << std::endl;
    }
    catch (Exception &e) {
        cerr << "Exception: " << e.what() << endl;
	cerr << "***** finished t3 test\n\n" << endl;
    }

    cerr << "***** starting t4 test" << endl;
	/* test a thread that throws an Exception in its run method.
	 * try to cancel it after it has thrown the exception.
	 */
	ThreadTestE t4("test4",false);
	t4.start();

	ts.tv_sec = 2;
	ts.tv_nsec = 0;
	nanosleep(&ts,0);
    try {
	t4.cancel();
    }
    catch (Exception &e) {
        cerr << "t4 cancel exception: " << e.what() << endl;
    }

    try {
	jres = t4.join();
	std::cerr << t4.getFullName() << " jres=" << jres << std::endl;
    }
    catch (Exception &e) {
        cerr << "t4 join exception: " << e.what() << endl;
    }
    cerr << "***** finished t4 test\n\n" << endl;

    // ts.tv_sec = 10;
    // nanosleep(&ts,0);
    return 0;
}
