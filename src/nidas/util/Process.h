/*              Copyright (C) 1989,90,91,92,93,94 by UCAR
 *
 * File       : $RCSfile: UTime.h,v $
 * Revision   : $Revision$
 * Directory  : $Source: /code/cvs/isa/src/lib/atdUtil/UTime.h,v $
 * System     : ASTER
 * Author     : Gordon Maclean
 * Date       : $Date$
 *
 * Description: class providing support for Unix process management:
 *      pid lock files, exec (eventually), etc.
 */

#ifndef NIDAS_UTIL_PROCESS_H
#define NIDAS_UTIL_PROCESS_H

#include <nidas/util/IOException.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <signal.h>
#include <sys/types.h>

#include <ext/stdio_filebuf.h>

namespace nidas { namespace util {

/**
 * Process provides an encapsulation of a spawned process, allowing the parent process
 * to perform I/O with the spawned process, send it signals and wait for it to finish.
 * Static methods are provided for spawning (forking and executing) a process.
 * A static method also exists to atomically check and create a process id file.
 */
class Process {
public:

    /**
     * Construct a Process.
     * @ param pid: pid should be > 0 and be a valid process id.
     * Unix system calls support doing kills and waits on a pid
     * less than or equal to 0, but those capabilities are
     * not supported in this class.  I/O via the getXXFd() or xxStream()
     * methods is not possible with a process created with this
     * constructor.  kill() and wait() are supported.
     * In order to use the Process methods to perform I/O with a process,
     * it must be started with one of the spawn methods.
     */
    Process(int pid);

    /**
     * Default constructor. Does not point to any process.
     */
    Process();

    /**
     * Copy constructor.  The new process will inherit
     * the file descriptors and iostreams from x.
     * The file descriptors and iostreams of the copied
     * Process, x, are no longer valid after this copy.
     */
    Process(const Process& x);

    /**
     * Assignment operator.  The assigned process (on LHS of =)
     * will inherit the file descriptors and iostreams from x.
     * The file descriptors and iostreams of the RHS Process, x,
     * are no longer valid after this assignment.
     */
    Process& operator=(const Process& x);

    /**
     * Destructor, which does very little. It does not do a
     * kill or wait on the process, or close
     * any file descriptors that are connected to the process.
     */
    ~Process();

    /**
     * Fork and execute a command and associated arguments and
     * environment. Return the Process object that can be
     * used to communicate with or control the sub-process.
     * @param niceval: The desired nice value, typically a positive
     *      value so that the spawned process has a higher nice
     *      value (lower priority).  If niceval is negative
     *      and the user does not have sufficient permisions
     *      to lower the nice value, the error is silently ignored.
     */
    static Process spawn(const std::string& cmd,
        const std::vector<std::string>& args,
        const std::map<std::string, std::string>& env,
        int niceval=0) throw(IOException);

    /**
     * Fork and execute a command and associated arguments.
     * Return the Process object that can be
     * used to communicate with or control the sub-process.
     */
    static Process spawn(const std::string& cmd,
        const std::vector<std::string>& args,
        int niceval=0) throw(IOException);

    /**
     * Execute a command by forking and executing command from the
     * shell command line using "sh -c cmd".  Return the Process.
     */
    static Process spawn(const std::string& cmd) throw(IOException);

    /**
     * Send a signal to the process.
     */
    void kill(int signal) throw(IOException);

    /**
     * Do a system wait on a process.
     * @return Process id of waited on process, or 0
     *  if hang=false and process has not finished,
     *  or -1 if process does not exist.
     *
     * @param hang: If true, wait for process state to change. If
     *     hang=false, don't wait, return 0 if process state has not changed.
     * @param status: status of process. See man 2 waitpid.
     * After a successfull wait, any file descriptors connected to the
     * process will be closed.
     */
    int wait(bool hang, int *status) throw(IOException);

    /**
     * Get the file descriptor of the pipe that is connected
     * to the standard in of the Process.  This file descriptor
     * is only valid after a spawn() of a Process.
     * Bytes writen to this file descriptor can be then
     * read by the Process.
     */
    int getInFd() const { return _infd; }

    /**
     * Get the ostream of the pipe that is connected
     * to the standard in of the Process.  This ostream is
     * only valid after a spawn() of a Process.
     */
    std::ostream& inStream() { return *_instream_ap.get(); }

    /**
     * Close the file descriptor and ostream of the pipe
     * connected to the standard in of the Process.
     * The Process will then receive an EOF on a subsequent read
     * from the other end of the pipe.
     */
    void closeIn();

    /**
     * Get the file descriptor of the pipe that is connected
     * to the standard out of the Process.  This file descriptor
     * is only valid after a spawn() of a Process.
     * The output of the Process can be read from this file descriptor.
     */
    int getOutFd() const { return _outfd; }

    /**
     * Get the istream of the pipe that is connected
     * to the standard out of the Process.  This istream
     * is only valid after a spawn() of a Process.
     * The output of the Process can be read from this istream.
     */
    std::istream& outStream() { return *_outstream_ap.get(); }

    /**
     * Close the file descriptor and istream of the pipe
     * connected to the standard out of the Process.
     */
    void closeOut();

    /**
     * Get the file descriptor of the pipe that is connected
     * to the standard error of the Process.  This file descriptor
     * is only valid after a spawn() of a Process.
     * The error output of the Process can be read from this file descriptor.
     */
    int getErrFd() const { return _errfd; }

    /**
     * Get the istream of the pipe that is connected
     * to the standard error of the Process.  This istream
     * is only valid after a spawn() of a Process.
     * The error output of the Process can be read from this istream.
     */
    std::istream& errStream() { return *_errstream_ap.get(); }

    /**
     * Close the file descriptor and istream of the pipe
     * connected to the standard error of the Process.
     */
    void closeErr();

    /**
     * The process id of the Process.
     * @return: Will be -1 if this Process is not associated with
     *      a valid process.
     */
    pid_t getPid() const { return _pid; }

    /**
     * Check if another process is running, using the named process id file.
     * A process id file typically has a name like /var/run/xxxx.pid,
     * or /tmp/xxxx.pid.  This function is useful when there should only be
     * one instance of a process running on a system - for
     * example a daemon which listens on a socket port.
     * The process name, "xxxx" should not be used by any other
     * process on the system.
     *
     * @return 0: means that no other process which uses file pidFile
     *      appears to be running. The pidFile was then created, containing
     *      the process id of the current process.
     *        >0: pid of a process that either holds a lock on pidFile,
     *          or pidFile contains a pid of a process that is still running.
     *
     *  If the return is 0, it means that the pidFile either didn't exist,
     *  was empty, didn't contain a number, or a current process with the
     *  given number doesn't exist. In this case, the file is created
     *  if necessary, and will contain the process id of the current
     *  process. checkPidFile also calls atexit() to schedule the
     *  removePidFile() function to be run when the current process exits.
     *  One typically does not have to call removePidFile explicitly.
     */
    static pid_t checkPidFile(const std::string& pidFile)
        throw(IOException);

    /** 
     * Remove the pid file. Note that checkPidFile schedules this
     * function to be called when the process exits.
     */
    static void removePidFile();

private:

    static std::string _pidFile;

    pid_t _pid;

    /**
     * File descriptor that is connected via a pipe to the
     * standard in file descriptor of a spawned process.
     * It is declared mutable to allow invalidating x._infd
     * in the copy constructor and assignment operators
     * that take a reference to a const Process argument.
     */
    mutable int _infd;

    void setInFd(int val);

    /**
     * GNU extension filebuf which is needed to create a ostream
     * from a file descriptor.
     */
    mutable std::auto_ptr<__gnu_cxx::stdio_filebuf<char> > _inbuf_ap;

    mutable std::auto_ptr<std::ostream> _instream_ap;

    /**
     * File descriptor that is connected via a pipe to the
     * standard out file descriptor of a spawned process.
     */
    mutable int _outfd;

    void setOutFd(int val);

    mutable std::auto_ptr<__gnu_cxx::stdio_filebuf<char> > _outbuf_ap;

    mutable std::auto_ptr<std::istream> _outstream_ap;

    /**
     * File descriptor that is connected via a pipe to the
     * standard error file descriptor of a spawned process.
     */
    mutable int _errfd;

    void setErrFd(int val);

    mutable std::auto_ptr<__gnu_cxx::stdio_filebuf<char> > _errbuf_ap;

    mutable std::auto_ptr<std::istream> _errstream_ap;
};

}}	// namespace nidas namespace util

#endif
