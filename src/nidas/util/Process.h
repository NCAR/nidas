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

#include <sys/types.h>

namespace nidas { namespace util {

class Process {
public:

    /**
     * Check if another process is running using the same process id file.
     * A process id file typically has a name like /var/run/xxxx.pid,
     * or /tmp/xxxx.pid.  This function is useful when there should only be
     * one instance of a process running on a system - for
     * example a daemon which listens on a socket port.
     * The process name, "xxxx" should not be used by any other
     * process on the system.
     *
     * @return 0: if no other process which uses file pidFile
     *      appears to be running, meaning pidFile either doesn't exist,
     *      is empty, does not contain a number, or a current process with the
     *      given number doesn't exist.
     *        >0: pid of process that either holds a lock on pidFile,
     *          or pidFile contains a pid of a process that is still running.
     * checkPidFile also calls atexit() to schedule the removePidFile()
     * function to be run when the process exits.
     * One typically does not have to call removePidFile explicitly.
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

};

}}	// namespace nidas namespace util

#endif
