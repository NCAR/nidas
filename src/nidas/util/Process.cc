
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-09-11 14:02:10 -0600 (Mon, 11 Sep 2006) $

    $LastChangedRevision: 3477 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/Process.cc $
 ********************************************************************
*/

#include <nidas/util/Process.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>

#include <iostream>

using namespace std;
using namespace nidas::util;

/* static */
string Process::_pidFile;

/* static */
pid_t Process::checkPidFile(const string& pidFile)
    throw(IOException)
{
    int fd;

    if ((fd = ::open(pidFile.c_str(),O_RDWR|O_CREAT,0644)) < 0)
        throw IOException(pidFile,"open",errno);

    // lock the file so that all the following operations
    // on the pid file are atomic - to exclude checkPidFile()
    // in other processes from reading or writing to the same file
    // until we are done.
    struct flock fl;
    for (;;) {
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;
        fl.l_pid = 0;
        if (::fcntl(fd,F_SETLK,&fl) == 0) break;    // successful lock
        if (errno != EACCES && errno != EAGAIN) {
            int ierr = errno;
            ::close(fd);
            throw IOException(pidFile,"fcntl(,F_SETLK,)",ierr);
        }
        // file is locked, get process id of locking process
        if (::fcntl(fd,F_GETLK,&fl) < 0) {
            int ierr = errno;
            ::close(fd);
            throw IOException(pidFile,"fcntl(,F_GETLK,)",ierr);
        }
        if (fl.l_type != F_UNLCK) {
            ::close(fd);
            // cerr << "file locked by pid " << fl.l_pid << endl;
            return fl.l_pid;
        }
        // F_GETLK has returned F_UNLCK in l_type, meaning the file
        // should now be lockable, so this shouldn't infinitely loop.
    }

    // read process id from file, check for /proc/xxxxx directory
    char procname[16];
    strcpy(procname,"/proc/");
    size_t l;
    if ((l = ::read(fd,procname+6,sizeof(procname) - 7)) < 0) {
        int ierr = errno;
        ::close(fd);
        throw IOException(pidFile,"read",ierr);
    }
    pid_t pid;
    // check that contents of file is numeric pid
    procname[l+6] = 0;
    if (procname[l+5] == '\n') procname[l+5] = '\0';
    if (::sscanf(procname+6,"%d",&pid) == 1) {
        struct stat statbuf;
        // check if a file /proc/xxxxx exists, where xxxxx is the pid.
        if (::stat(procname,&statbuf) == 0) {   // process exists
            // assert(S_ISDIR(statbuf.st_mode));
            ::close(fd);
            return pid;
        }
    }
    // write current pid to file
    pid = getpid();
    sprintf(procname,"%d\n",pid);
    if (::lseek(fd,0,0) < 0 || ::write(fd,procname,strlen(procname)) < 0) {
        int ierr = errno;
        ::close(fd);
        throw IOException(pidFile,"write",ierr);
    }
    ::close(fd);
    _pidFile = pidFile;
    atexit(removePidFile);
    return 0;
}

/* static */
void Process::removePidFile()
{
    // ignore errors
    if (_pidFile.length() > 0) ::unlink(_pidFile.c_str());
}
