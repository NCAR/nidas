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

#ifndef NIDAS_UTIL_THREAD_H
#define NIDAS_UTIL_THREAD_H

#include <csignal>

#include <nidas/util/Exception.h>
#include <nidas/util/ThreadSupport.h>

#include <string>
#include <map>
#include <set>

namespace nidas { namespace util {

class Runnable {
public:

    virtual ~Runnable() {}
    /**
     * The method which will run in its own thread.  This method must
     * be supplied by the Runnable or Thread subclass.
     **/
    virtual int run() throw(Exception) = 0;

    /**
     * Interrupt this run method.  This sets a flag which can be tested with
     * isInterrupted().  It is up to the run() implementation to return 
     * when interrupted.
     **/
    virtual void interrupt() = 0;

    /**
     * Has the run method been interrupted?
     **/
    virtual bool isInterrupted() const = 0;

protected:
    /**
     * Check if we have been cancelled. Calls ::pthread_testcancel.
     * This is protected since it only checks the current thread -
     * i.e. it must be called within the run method.
     * Since it just calls ::pthread_testcancel, it is a cancellation point.
     */
    void testCancel() const { ::pthread_testcancel(); }

    /**
     * Call testCancel, and return true when this thread has been interrupted.
     * This is protected because it should only called within the run method
     * of the thread.
     **/
    virtual bool amInterrupted() const;
};

class Thread : public Runnable {

public:
    /** 
     * Return the thread object for the current thread.  Returns null if
     * not found.
     **/
    static Thread *currentThread ();

    static pthread_t currentThreadId ();

    /**
     * Convenience routine to return the name for the current thread,
     * or a string indicating that the name of the thread is unknown. 
     **/
    static const std::string& currentName ()
    {
        Thread *thr = currentThread ();
        if (thr) return thr->getName();
        return unknownName;
    }

    /**
     * Values that can be returned by run method. User can define other
     * values greater than RUN_EXCEPTION. These values are then returned by
     * <code>int join()</code>. 
     * Note that PTHREAD_CANCELLED is -1
     */
    enum runStatus { RUN_CANCELED = -1, RUN_OK = 0, NOT_RUNNING = 1, RUN_EXCEPTION = 2 };

public:

    /**
     * Constructor for a thread, giving it a name. This does not
     * start a processor thread. Use the Thread::start() method
     * to start the processor thread.
     * By default, SIGINT, SIGTERM and SIGHUP signals will be
     * blocked when the thread starts. Use unblockSignal(sig) to
     * unblock them if desired.
     */
    Thread(const std::string& name,bool detached=false);

    /**
     * Copy constructor. Does not create a separate processor thread if
     * the original thread is running.
     */
    Thread(const Thread& x);

    /**
     * Thread destructor.  The destructor does not join or cancel
     * a thread.
     *
     * Users should not invoke the destructor of a detached thread
     * after it has been started.  Detached threads call their own
     * destructor when finished.
     *
     * The following comments pertain to non-deattached threads.
     *
     * A thread shouldn't be running at the time of destruction.
     *
     * Non-detached threads also should have been joined before
     * the destructor is called.
     *
     * This destructor will complain to cerr if either of the two
     * above situations exists.  These situations are errors on the
     * part of the user of Thread and should be corrected.
     */
    virtual ~Thread();

    /**
     * Start the thread running, meaning execute the run method in a
     * separate thread.
     */
    virtual void start() throw(Exception);

    /**
     * The calling thread joins this thread, waiting until the thread
     * finishes, which means either that the run() method returned,
     * the thread called pthread_exit() or the thread was cancelled.
     * The return value is the int return value of the run method,
     * or PTHREAD_CANCELED (-1).  If the run method threw an Exception,
     * it will be caught and then re-thrown by join.
     */
    virtual int join() throw(Exception);

    /**
     * Send a signal to this thread.
     */
    virtual void kill(int sig) throw(Exception);

    /**
     * Cancel this thread.
     */
    virtual void cancel() throw(Exception);

    /**
     * Interrupt this thread.  This sets a boolean which can be tested with
     * isInterrupted().  It is up to the run() implementation to return 
     * when interrupted.  This is a "soft" request to terminate the thread.
     *
     * What follows is a discussion of when to use cancel(), kill(), or
     * interrupt() to terminate a thread.
     *
     * If you can consistently check the state of isInterrupted() in the run
     * method, and return if it is true, at a time interval which
     * is an acceptable amount of time to wait for the thread to
     * terminate, then using interrupt() should work well.
     *
     * If the run method does I/O, then things are usually a bit
     * more complicated.
     *
     * If all I/O is guaranteed to finish quickly, which is a rare
     * situation, or is done with a timeout, such as using select/poll
     * with a timeout before every read/write, then one could still
     * use interrupt() as above, and the thread will terminate within
     * the timeout period.
     *
     * If the thread does blocking I/O operations, and the I/O might
     * block for a period of time longer than you're willing to wait
     * for the thread to terminate, then you need to use kill(sig)
     * or cancel().
     *
     * If a signal is sent to the thread with kill(sig) while the
     * thread is blocking on an I/O operation, the I/O will
     * immediately return with an errno of EINTR, and one could then
     * return from the run method, after any necessary cleanup.
     *
     * However there is a possibility that the signal could be missed.
     * In order to make sure you receive a signal, you must block it,
     * so that any received signals are held as pending,
     * and then atomically unblock it with pselect/ppoll/epoll_pwait
     * before performing the I/O operation.
     *
     * If you do not use I/O timeouts, or kill(sig) with
     * pselect/ppoll/epoll_pwait, or other tricks such as writing
     * to a pipe that is watched with select/poll in the run method,
     * then using cancel() is the only way to guarantee that your
     * thread will terminate in an acceptable amount of time.
     *
     * All NIDAS Threads support deferred cancelation.  Immediate
     * asynchronous cancellation is not fully supported (and is
     * very hard to get right).  Deferred cancelation means that
     * cancellation is delayed until the thread next calls a system
     * function that is a cancellation point.  At that point the
     * thread run method will simply terminate without any return
     * value from the system function, and then execute
     * any cleanup methods that may have been registered with
     * pthread_cleanup_push.
     *
     * A list of cancellation points is provided in the pthreads(7) man
     * page. Cancellation points are typically I/O operations, waits or sleeps.
     *
     * This immediate thread termination can be a problem if there
     * is a possibility that your objects could be left in a bad state,
     * such as with a mutex locked, though it isn't generally a good practice
     * to hold mutexes during a time-consuming I/O operation, wait or sleep.
     *
     * One should check the run method to see if the state of the
     * objects is OK if execution stops at any of the cancellation points.
     * Note that logging a message, or writing to cerr is a cancellation
     * point.  Typically there is error/exception handling associated with
     * an I/O operation. Ensuring the state is OK upon a cancellation
     * is similar to preparation for a fatal I/O error that requires
     * a return of the run method.  One can use setCancelEnabled() to
     * defer cancellation.
     *
     * One can use pthread_cleanup_push and pthread_cleanup_pop to
     * register cleanup routines that are called when a thread is
     * cancelled if special handling is required.
     */
    virtual void interrupt();

    /**
     * Return true when this thread has been interrupted.
     **/
    virtual bool isInterrupted() const
    { 
        return _interrupted; 
    }

    /**
     * Is this thread running?
     */
    virtual bool isRunning() const
    { 
        Synchronized sync(_mutex);
        return _running;
    }

    /**
     * Has this thread been joined?
     */
    virtual bool isJoined() const
    { 
        return getId() == 0;
    }

    /**
     * Is this a detached thread.
     */
    virtual bool isDetached() const
    { 
        return _detached; 
    }

    /**
     * Return true if the cancel state of this thread is PTHREAD_CANCEL_ENABLE.
     */
    bool isCancelEnabled() const;

    /**
     * Return true if the cancel type of this thread is
     * PTHREAD_CANCEL_DEFERRED.
     */
    bool isCancelDeferred() const;

    /**
     * Return the name of this thread.
     **/
    const std::string& getName() const throw();

    /**
     * Return a name with a bunch of descriptive fields, specifying
     * whether it is detached, the real-time priority, etc.
     **/
    const std::string& getFullName() throw();

    /**
     * Convenience function to return a string for the given scheduler policy:
     * "Non-RT", "RT:FIFO", "RT:RR" or "RT:Unknown".
     */
    static std::string getPolicyString(int policy);

    enum SchedPolicy { NU_THREAD_OTHER=SCHED_OTHER,
        NU_THREAD_FIFO=SCHED_FIFO, NU_THREAD_RR=SCHED_RR};

    bool setRealTimeRoundRobinPriority(int val) throw(Exception);
    bool setRealTimeFIFOPriority(int val) throw(Exception);
    bool setNonRealTimePriority() throw(Exception);

    void setThreadScheduler(enum SchedPolicy policy, int priority) throw(Exception);

    /**
     * Block a signal in this thread. This method is usually called
     * before this Thread has started. If this Thread is currently
     * running, then this method is only effective if called from this
     * Thread, i.e. from its own run() method.
     *
     * Because SIGINT, SIGTERM and SIGHUP are typically caught in 
     * the main thread, they are blocked by default in a Thread.
     * Call unblockSignal(sig) if you want to catch them in a Thread.
     */
    void blockSignal(int);

    /**
     * Install a signal handler and unblock the signal.
     *
     * The signal handler will log a message about the receipt of the signal
     * at severity LOG_INFO using the nidas::util::Logger.
     * Then, if the signal handler is being invoked from a registered Thread,
     * the virtual method signalHandler() for that Thread will be called.
     *
     * The signal handler is installed with the sigaction() system call, and
     * will be the action for the given signal in all threads, including
     * the main() thread.  If other threads do not wish to take action on
     * a given signal, they should call blockSignal(sig).  Or they can
     * define their own signalHandler() method.
     *
     * After installing the signal handler, the signal is added to those that are
     * unblocked for the thread, or if the Thread is not yet running, the signal
     * will be unblocked in the thread once it runs.
     *
     * As with blockSignal(), this method is typically called on this
     * Thread before it has started.  If this Thread has started, then
     * the signal will only be unblocked if the method is called
     * from this Thread, i.e. from its own run() method.
     *
     * To install a signal handler, and then block the signal so
     * that it is held as pending until it is later unblocked, typically
     * with pselect(), or sigwaitinfo(), do:
     *
     * \code
     * void Thread::run() 
     * {
     *     // get the existing signal mask
     *     sigset_t sigmask;
     *     pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
     *     // remove SIGUSR1 from the mask passed to pselect
     *     sigdelset(&sigmask,SIGUSR1);
     *     
     *     for (;;) {
     *         pselect(nfd,&readfds,0,0,0,&sigmask);
     *         ...
     *     }
     * }
     * ...
     * thread.unblockSignal(SIGUSR1);
     * thread.blockSignal(SIGUSR1);
     * thread.start();
     * ...
     * try {
     *     if (thread.isRunning()) {
     *         thread.kill(SIGUSR1);
     *         thread.join()
     *     }
     * }
     * \endcode
     */
    void unblockSignal(int);

    /**
     * a test method.
     */
    static int test(int argc, char** argv);

protected:
    /**
     * Set the cancel state for this thread - false means cancel
     * requests are ignored. See ::pthread_setcancelstate.
     * This is protected, it should be called only from a thread's
     * own run method.
     */
    void setCancelEnabled(bool val);

    /**
     * Set the cancel type for this thread. true means cancel requests
     * are deferred until the next cancellation point. false means
     * they occur instantly.  This is protected, it should be called
     * only from a thread's own run method.
     * See the pthreads(7) man page for a list of the cancellation points.
     *
     * Note: non-deferred canceling is difficult to get right.
     * It has not been tested with this class, and is not
     * recommended.
     */
    void setCancelDeferred(bool val);

    pthread_t getId() const
    {
        Synchronized sync(_mutex);
        return _id;
    }

private:

    static void* thr_run(void *me);
    static void* thr_run_detached(void *me);
    static void thr_cleanup(void *me);
    static void thr_cleanup_delete(void *me);
    static void thr_add_sig(int sig);

    /**
     * Signal handler function for this thread.  The default
     * handler just sets _interrupt to true, so that amInterrupted()
     * or isInterrupted() will return true.  Derived classes can
     * override this method for custom signal handling.
     * However, derived classes are limited in what they can do
     * in their signal handler. Specifically, from the pthread_cond_signal
     * man page:
     *
     * "It  is  not  safe to use the pthread_cond_signal() function in
     * a signal handler that is invoked asynchronously."
     *
     * Therefore do not call Cond::signal() from a signal handler.
     */
    virtual void signalHandler(int /* sig */, siginfo_t* /* si */)
    {
        _interrupted = true;
    }

    static std::string unknownName;

    virtual int pRun ();

    void setThreadSchedulerNolock(enum SchedPolicy policy, int priority) throw(Exception);

    void makeFullName();		// add thread id to name once we know it

    /**
     * Register this current thread with a static registry of threads
     * by id.
     * <code>
     * Thread::registerThread (new Thread ("Main"));
     * </code>
     **/
    void registerThread();

    void unregisterThread();

    /**
     * Mutex for accessing _id.
     */
    mutable Mutex _mutex;

    std::string _name;

    std::string _fullname;

    pthread_t _id;

    bool _running;

    bool _interrupted;

    bool _cancel_enabled;

    bool _cancel_deferred;

    pthread_attr_t _thread_attr;

    /**
     * Exception thrown by run method.
     */
    Exception* _exception;

    bool _detached;

    sigset_t _blockedSignals;

    sigset_t _unblockedSignals;

    static void sigAction(int sig, siginfo_t* si,void* vptr);

    void unsetId()
    {
        Synchronized sync(_mutex);
        _id = 0;
    }

    /**
     * Assignment operator not supported. Supporting it wouldn't
     * be much problem, since the copy constructor is supported,
     * but there hasn't been a need yet.
     */
    Thread& operator=(const Thread& x);

};

/**
 * A Thread with a constructor that sets detached=true.
 */
class DetachedThread : public Thread {
public:
    DetachedThread(const std::string& name) : Thread(name,true) {}
};

/**
 * The ThreadRunnable class implements a Thread which uses a Runnable
 * target to supply the run() method.  It also serves as a "default thread"
 * if it has no target.  In that case the run() method does nothing; it
 * just returns.
 **/
class ThreadRunnable : public Thread
{
public:
    ThreadRunnable (const std::string& name, Runnable* target = 0) :
        Thread (name),
        _target(target)
    {}

    void interrupt() {
        if (_target) _target->interrupt();
    }

    int
        run () throw(Exception)
        {
            if (_target)
                return _target->run();
            return NOT_RUNNING;
        }

private:
    Runnable *_target;

    /** No copy */
    ThreadRunnable(const ThreadRunnable&);

    /** No assignment */
    ThreadRunnable& operator=(const ThreadRunnable&);

};

/**
 * In certain situations one needs to "join oneself", which
 * would be a deadlock. ThreadJoiner is the solution.
 * It joins and then deletes the thread.
 * Since it is a detached thread it itself doesn't need to be
 * joined - there is an end to it all!
 */
class ThreadJoiner: public DetachedThread
{
public:
    ThreadJoiner(Thread* thrd);

    ~ThreadJoiner();

    int run() throw();
private:
    Thread* _thread;

    /** No copy */
    ThreadJoiner(const ThreadJoiner&);

    /** No assignment */
    ThreadJoiner& operator=(const ThreadJoiner&);
};

typedef ThreadRunnable DefaultThread;

}}	// namespace nidas namespace util

#endif

