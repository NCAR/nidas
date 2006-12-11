//
//              Copyright 2004 (C) by UCAR
//

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
   * Return true when this thread has been interrupted. This is protected
   * so that it is only called within a run method (it may call testCancel).
   **/
  virtual bool amInterrupted() const {
    testCancel();
    return isInterrupted();
  }


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

  typedef std::map<pthread_t, Thread *, std::less<pthread_t> > threadmap_t;

  static Mutex _threadsMutex;
  static threadmap_t _threads;
  static std::set<int> _handledSignals;

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
   * after it has been started.  It will call its own destructor
   * when it's finished.
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
   * Send a signal to this thread. In order to catch this signal
   * this thread must have done an unblockSignal(sig). Otherwise
   * the default system action (terminate, core dump, stop)
   * will be performed.
   */
  virtual void kill(int sig) throw(Exception);

  /**
   * Cancel this thread.
   */
  virtual void cancel() throw(Exception);

  /**
   * Interrupt this thread.  This sets a flag which can be tested with
   * isInterrupted().  It is up to the run() implementation to return 
   * when interrupted.  This is a "soft" cancel.
   **/
  virtual void interrupt();

  /**
   * Signal handler function for this thread.  The default
   * handler just calls interrupt.  Derived classes
   * can override this method for custom signal handling.
   * A signal must be unblocked, see unblockSignal(), in order
   * for this signalHandler to be called on receipt of a signal.
   */
  virtual void signalHandler(int sig, siginfo_t* si) {
      interrupt();
  }

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
    return _running; 
  }

  /**
   * Has this thread been joined?
   */
  virtual bool isJoined() const
  { 
    return !_needsjoining; 
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
  const std::string getFullName() throw();

  bool setRealTimeRoundRobinPriority(int val) throw(Exception);
  bool setRealTimeFIFOPriority(int val) throw(Exception);
  bool setNonRealTimePriority() throw(Exception);

  /**
   * Block this signal.
   */
  void blockSignal(int);

  /**
   * Unblock this signal. When this signal is received by this thread,
   * the signalHandler() method will be called. This will install
   * a signal handler, using the sigaction() system function,
   * for all threads, including the main() thread.  If other
   * threads do not wish to be notified of this signal,
   * then they should call blockSignal(sig).
   */
  void unblockSignal(int);

  /**
   * a test method.
   */
  static int test(int argc, char** argv);

protected:
  static void* thr_run(void *me);
  static void* thr_run_detached(void *me);
  static void thr_delete(void *me);
  static void thr_cleanup(void *me);
  static void thr_add_sig(int sig);

  static std::string unknownName;

  virtual void*
  pRun ();

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
   * The cancellation points are:
   *    pthread_join(3)
   *    pthread_cond_wait(3)
   *    pthread_cond_timedwait(3)
   *    pthread_testcancel(3)
   *    sem_wait(3)
   *    sigwait(3)
   * Since cancellation is implemented with signals, system calls
   * like read(2),write(2), wait(2), select(2) will return EINTR
   * on a cancel, and one can do testCancel() at that point.
   *
   * Note: non-deferred canceling is difficult to get right.
   * It has not been tested with this class, and is not
   * recommended.
   */
  void setCancelDeferred(bool val);

  /**
   * Register this current thread with a static registry of threads
   * by id.
   * <code>
   * Thread::registerThread (new Thread ("Main"));
   * </code>
   **/
  void registerThread();
  void unregisterThread();

  void setThreadScheduler(int policy, int priority) throw(Exception);

  std::string fullName();

  void makeFullName();		// add thread id to name once we know it

  pthread_t getId() const { return _id; }

protected:

  Mutex _mutex;         // Mutex for private structure
  Mutex _joinMutex;	// synchronizing between join and pthread_cleanup func
  Mutex _fullnamemutex;     // Mutex for building the full name
  std::string _name;
  std::string _fullname;
  pthread_t _id;
  bool _interrupted;
  bool _cancel_enabled;
  bool _cancel_deferred;
  bool _running;
  bool _needsjoining;
  pthread_attr_t thread_attr;
  /**
   * Exception thrown by run method.
   */
  Exception* _exception;
  bool _detached;
  sigset_t blockedSignals;
  sigset_t unblockedSignals;

protected:
  static void sigAction(int sig, siginfo_t* si,void* vptr);

private:
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
    Thread* thread;
};

typedef ThreadRunnable DefaultThread;

}}	// namespace nidas namespace util

#endif

