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

/* needed with gcc 2.X and strsignal */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/Logger.h>

#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <pthread.h>
#include <cstring>
#include <csignal>
#include <errno.h>

#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace nidas::util;


typedef std::map<pthread_t, Thread *, std::less<pthread_t> > threadmap_t;

namespace 
{
    threadmap_t _threads;

    Mutex _threadsMutex;

    /**
     * the signals that we handle because the user has called unblockSignal()
     * on a thread.
     */
    set<int> _handledSignals;
}

bool Runnable::amInterrupted() const
{
    testCancel();
    return isInterrupted();
}

void
Thread::sigAction(int sig,siginfo_t* siginfo,void*)
{
    pthread_t id = Thread::currentThreadId();
    Thread *thrptr = Thread::currentThread();

    if (thrptr) {
        DLOG(("") << "thread " << thrptr->getName() << 
                " received signal " << strsignal(sig) << "(" << sig << ")" << 
                " si_signo=" << siginfo->si_signo << 
                " si_errno=" << siginfo->si_errno << 
                " si_code=" << siginfo->si_code);
        thrptr->signalHandler(sig,siginfo);
    }
    else {
        DLOG(("") << "unknown thread " << "(" << id << ")" <<
                " received signal " << strsignal(sig) << "(" << sig << ")" << 
                " si_signo=" << siginfo->si_signo << 
                " si_errno=" << siginfo->si_errno << 
                " si_code=" << siginfo->si_code);
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
    if (_id) _threads[_id] = this;
}

void
Thread::unregisterThread()
{
    Synchronized sync(_threadsMutex);
    const threadmap_t::iterator& it = _threads.find(_id);
    if (it != _threads.end()) _threads.erase(it);
}

Thread::Thread(const std::string& name, bool detached) : 
    _mutex(),
    _name(name),
    _fullname(name),
    _id(0),
    _running(false),
    _interrupted(false),
    _cancel_enabled(true),
    _cancel_deferred(true),
    _thread_attr(),
    _exception(0),
    _detached(detached),
    _blockedSignals(),
    _unblockedSignals()
{
    ::pthread_attr_init(&_thread_attr);
    ::pthread_attr_setdetachstate(&_thread_attr,
            _detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
    sigemptyset(&_unblockedSignals);

    // By default block signals that typically one wants to catch
    // in the main thread.
    sigemptyset(&_blockedSignals);
    sigaddset(&_blockedSignals,SIGINT);
    sigaddset(&_blockedSignals,SIGTERM);
    sigaddset(&_blockedSignals,SIGHUP);
}


/* Copy constructor */
Thread::Thread(const Thread& x):
    Runnable(),
    _mutex(x._mutex),
    _name(x._name),
    _fullname(x._fullname),
    _id(0),
    _running(false),
    _interrupted(false),
    _cancel_enabled(x._cancel_enabled),
    _cancel_deferred(x._cancel_deferred),
    _thread_attr(),
    _exception(0),
    _detached(x._detached),
    _blockedSignals(x._blockedSignals),
    _unblockedSignals(x._unblockedSignals)
{
    ::pthread_attr_init(&_thread_attr);
    ::pthread_attr_setdetachstate(&_thread_attr,
            _detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
}

Thread::~Thread()
{
    delete _exception;
    ::pthread_attr_destroy(&_thread_attr);

    if (isRunning()) {
        Exception e(string("thread ") + getName() +
                " still running at destruction time");
        PLOG((e.what()));
        // throw e;
    }
    if (!_detached && !isJoined()) {
        Exception e(string("thread ") + getName() +
                " not joined at destruction time");
        PLOG((e.what()));
        // throw e;
    }
}

void Thread::blockSignal(int sig) {
    // set the signal masks which are then
    // applied to the thread at the beginning of the run method.
    sigaddset(&_blockedSignals,sig);
    sigdelset(&_unblockedSignals,sig);

    // It is somewhat counter-intuitive to setup a signal handler
    // on signals that are to be blocked, but it is often
    // what is wanted.  A Thread may block a signal so that it later
    // can be unblocked in a call to pselect, ppoll, etc.
    //
    // If a signal handler is not set for a signal,
    // and the default disposition is to terminate the process,
    // which is the case with SIGUSR1, for example, then
    // according to the pthread_kill man page:
    // "this action will affect the whole process".
    // So, install a handler...
    thr_add_sig(sig);

    // pthread_sigmask changes the signal mask of the current thread.
    // so we check that the current thread is this thread.
    if (isRunning() && Thread::currentThread() == this)
        ::pthread_sigmask(SIG_BLOCK,&_blockedSignals,(sigset_t*)0);
}

void Thread::unblockSignal(int sig) {
    sigaddset(&_unblockedSignals,sig);
    sigdelset(&_blockedSignals,sig);
    thr_add_sig(sig);
    if (isRunning() && Thread::currentThread() == this) {
        ::pthread_sigmask(SIG_UNBLOCK,&_unblockedSignals,(sigset_t*)0);
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
void Thread::thr_cleanup_delete(void *me) 
{ 
    Thread *thr = (Thread*)me;

    thr->_mutex.lock();

    thr->unregisterThread();
    thr->_id = 0;
    thr->_running = false;

    // must unlock this mutex before deleting the thread,
    // since the destructor locks it again.
    thr->_mutex.unlock();


#ifdef DEBUG
    // Detached threads may be running after the process main has finished,
    // at the same time that static objects are being destroyed.  So don't
    // use Logger to send this finished message.
    // this write to cerr is a cancelation point, so disable cancelation
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldstate);
    cerr << thr->getFullName() << " run method finished" << endl;
    pthread_setcancelstate(oldstate,0);
#endif

    delete thr;
}

/* static */
void Thread::thr_cleanup(void *me) 
{ 
    Thread *thr = (Thread*)me;

    Synchronized sync(thr->_mutex);

    thr->_running = false;
    thr->unregisterThread();

    // this log message is a cancelation point, so disable cancelation
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldstate);
    ILOG(("") << thr->getFullName() << " run method finished");
    pthread_setcancelstate(oldstate,0);
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

    /* thr_cleanup will be called whenever this thread quits */
    pthread_cleanup_push(thr_cleanup,me);

    // At this point we have entered the new thread, but we are in a static
    // method.  So pass the work back into our thread object instance so it
    // can access its own members and call its run method.

    result = (void*)(long) thisThread->pRun();

    /* pop and execute the cleanup hander */
    pthread_cleanup_pop(1);

    pthread_exit(result);
}

/* static */
void* Thread::thr_run_detached(void *me) 
{ 
    void* result = 0;
    Thread *thisThread = (Thread *)me;

    // At this point we have entered the new thread, but we are in a static
    // method.  So pass the work back into our thread object instance so it
    // can access its own members and call its run method.

    // Cleanup and delete the Thread object, on cancellation or 
    // if the run method finishes.

    pthread_cleanup_push(thr_cleanup_delete,thisThread);

    result = (void*) (long) thisThread->pRun();

    pthread_cleanup_pop(1);

    return result;
}

int
Thread::pRun()
{
    /* wait until ::start has finished, so that the id is initialized. */
#ifdef SUPPORT_ASYNC_CANCEL
    pthread_cleanup_push_defer_np(
            (void(*)(void*))::pthread_mutex_unlock,(void*)_mutex.ptr());
    _mutex.lock();
    pthread_cleanup_pop_restore_np(1);
#else
    // by syncing on _mutex here we know the ::start method is finished.
    getId();
#endif

    delete _exception;
    _exception = 0;

    ::pthread_sigmask(SIG_UNBLOCK,&_unblockedSignals,(sigset_t*)0);
    ::pthread_sigmask(SIG_BLOCK,&_blockedSignals,(sigset_t*)0);

    ILOG(("") << getFullName() << " run...");

    int result = RUN_EXCEPTION;

    try
    {
        result = this->run();
    }
    catch (const Exception& ex)
    {
        _exception = ex.clone();
        result = RUN_EXCEPTION;
    }

    return result;
}

void
Thread::start() throw(Exception)
{
    Synchronized sync(_mutex);
    if (!_id)
    {
        int status = 0;

        /*
         * pthread_create can start the thr_run method before it sets the value of
         * the _id argument.  So the value of _id may
         * not be set if the run method needs it right away.
         * You must sync on _mutex in ::pRun to avoid this.
         */

        int state = 0;
        ::pthread_attr_getdetachstate( &_thread_attr, &state);

        for (int i = 0; i < 2; i++) {
            if (state == PTHREAD_CREATE_DETACHED)
                status = ::pthread_create(&_id, &_thread_attr,
                        thr_run_detached, this);
            else
                status = ::pthread_create(&_id, &_thread_attr,
                        thr_run, this);

            if (!status) break;
            _id = 0;
            // If we get a permissions problem in asking for a real-time 
            // schedule policy, then warn about the problem and ask for non
            // real-time.
            int policy;
            ::pthread_attr_getschedpolicy( &_thread_attr, &policy);

            if (status != EPERM || (policy != NU_THREAD_FIFO && policy != NU_THREAD_RR))
                break;
            ILOG(("") << getName() << ": start: " <<
                    Exception::errnoToString(status) << ". Trying again with non-real-time priority.");
            setThreadSchedulerNolock(NU_THREAD_OTHER,0);
        }

        if (status) 
            throw Exception(getName(),
                    string("pthread_create: ") + Exception::errnoToString(status));

        makeFullName();
        registerThread();

        _running = true;
        _interrupted = false;
    }
}

/*
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
    int status;
    void* thread_return = (void *)RUN_OK;

    pthread_t id = getId();

    /*
     * pthread_join will return EINVAL if thread is detached, ESRCH if
     * no thread with that ID found, EINVAL if another thread is
     * already waiting, or EDEADLK if I'm waiting on myself.
     */
    if (id && (status = ::pthread_join(id, &thread_return))) 
    {
        throw Exception(getName(),
                string("pthread_join: ") + Exception::errnoToString(status));
    }

    Synchronized sync(_mutex);
    _id = 0;
    if (thread_return == (void *) RUN_EXCEPTION && _exception)
        throw *_exception;
    return (long) thread_return;
}

void
Thread::kill(int sig) throw(Exception)
{
    int status;
    Synchronized sync(_mutex);
    if (_id && !(status = ::pthread_kill(_id,0)) &&
            (status = ::pthread_kill(_id,sig)))
    {
        throw Exception(getName(),string("pthread_kill: ") + Exception::errnoToString(status));
    }
}

/*
 * Queue a cancel request to this thread.
 */
void
Thread::cancel() throw(Exception)
{
    int status;
    Synchronized sync(_mutex);
    if (_id && (status = ::pthread_cancel(_id))) {
            throw Exception(getName(),string("pthread_cancel: ") +
                Exception::errnoToString(status));
    }
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

/* static */
string Thread::getPolicyString(int policy)
{
    switch (policy) {
    case NU_THREAD_OTHER: return "Non-RT";
    case NU_THREAD_FIFO:  return "RT:FIFO";
    case NU_THREAD_RR:    return "RT:RR";
    default: break;
    }
    return "RT:Unknown";
}

void Thread::makeFullName()
{
    std::ostringstream os;

    pthread_t id = _id;

    os << _name << "(id=" << id;

    sched_param param;
    int policy = 0;

    if (::pthread_getschedparam(id,&policy,&param) == 0)
        os << ',' << getPolicyString(policy);
    os << ",prior=" << param.sched_priority;

    if (_detached) os << ",detached";
    else os << ",joinable";

    if (!_cancel_enabled) os << ",cancel=no";
    else {
        if (_cancel_deferred) os << ",cancel=deferred";
        else os << ",cancel=immed";
    }
    os << ")";
    _fullname = os.str();
}

const std::string&
Thread::getFullName() throw()
{
    return _fullname;
}


void
Thread::interrupt()
{
    _interrupted = true; 
}


bool Thread::setRealTimeRoundRobinPriority(int val) throw(Exception) {
    // if (getuid() != 0) != 0) return false;
    setThreadScheduler(NU_THREAD_RR,val);
    return true;
}

bool Thread::setRealTimeFIFOPriority(int val) throw(Exception) {
    // if (geteuid() != 0) != 0) return false;
    setThreadScheduler(NU_THREAD_FIFO,val);
    return true;
}

bool Thread::setNonRealTimePriority() throw(Exception) {
    setThreadScheduler(NU_THREAD_OTHER,0);
    return true;
}

void Thread::setThreadScheduler(enum SchedPolicy policy,int val) throw(Exception) {
    Synchronized autolock(_mutex);
    setThreadSchedulerNolock(policy,val);
}

void Thread::setThreadSchedulerNolock(enum SchedPolicy policy,int val) throw(Exception)
{
    int status;
    sched_param param = sched_param();
    param.sched_priority = val;

    if (_id) {
        status = ::pthread_setschedparam(_id,policy,&param);
        if (status)
            throw Exception(getName(),
                    string("pthread_setschedparam: ") + Exception::errnoToString(status));
    }
    else {
        status = ::pthread_attr_setschedpolicy(&_thread_attr,policy);
        // int maxprior = sched_get_priority_max(policy);
        // cerr << "sched_get_priority_max(policy)=" << sched_get_priority_max(policy) << endl;
        if (status)
            throw Exception(getName(),
                    string("pthread_attr_setschedpolicy: ") + Exception::errnoToString(status));
        status = ::pthread_attr_setschedparam(&_thread_attr,&param);
        if (status)
            throw Exception(getName(),
                    string("pthread_attr_setschedparam: ") + Exception::errnoToString(status));
        status = ::pthread_attr_setinheritsched(&_thread_attr,PTHREAD_EXPLICIT_SCHED);
        if (status)
            throw Exception(getName(),
                    string("pthread_attr_setinheritsched: ") + Exception::errnoToString(status));
    }
}

ThreadJoiner::ThreadJoiner(Thread* thrd):
    DetachedThread("ThreadJoiner"),_thread(thrd)
{
}
ThreadJoiner::~ThreadJoiner()
{
}
int ThreadJoiner::run() throw() {
    try {
        _thread->join();
    }
    catch (const Exception& e) {
        PLOG(("") << _thread->getName() << ": " << e.what());
    }
#ifdef DEBUG
    cerr << "joined " << _thread->getName() << " deleting" << endl;
#endif
    delete _thread;
    return RUN_OK;
}

/* static */
int Thread::test(int, char**)
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
