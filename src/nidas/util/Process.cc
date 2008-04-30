
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/util/Process.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>
#include <cstring>

#include <iostream>
#include <stdlib.h>  // atexit()

using namespace std;
using namespace nidas::util;

/* static */
string Process::_pidFile;

namespace {
class OpenedFile
{
public:
    OpenedFile(int f): fd(f) {}
    ~OpenedFile() { ::close(fd); }
private:
    int fd;
};
}

/* static */
pid_t Process::checkPidFile(const string& pidFile)
    throw(IOException)
{
    int fd;
    if ((fd = ::open(pidFile.c_str(),O_RDWR|O_CREAT,0666)) < 0)
        throw IOException(pidFile,"open",errno);

    // create local variable which does a ::close on the file
    // descriptor in the destructor, so we don't have to remember.
    OpenedFile autofd(fd);

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
        if (errno != EACCES && errno != EAGAIN)
            throw IOException(pidFile,"fcntl(,F_SETLK,)",errno);

        // file is locked, get process id of locking process
        if (::fcntl(fd,F_GETLK,&fl) < 0)
            throw IOException(pidFile,"fcntl(,F_GETLK,)",errno);
        
        if (fl.l_type != F_UNLCK) {
            // cerr << "file locked by pid " << fl.l_pid << endl;
            return fl.l_pid;
        }
        // F_GETLK has returned F_UNLCK in l_type, meaning the file
        // should now be lockable, so this shouldn't infinitely loop.
        // This means the file was unlocked by another process between the
        // F_SETLK and F_GETLK calls above - not bloody likely, but hey...
    }

    // read process id from file, check for /proc/xxxxx directory
    char procname[16];
    strcpy(procname,"/proc/");
    size_t l;
    if ((l = ::read(fd,procname+6,sizeof(procname) - 7)) < 0)
        throw IOException(pidFile,"read",errno);
    pid_t pid;
    // check that contents of file is numeric pid
    procname[l+6] = 0;
    if (procname[l+5] == '\n') procname[l+5] = '\0';
    if (::sscanf(procname+6,"%d",&pid) == 1) {
        struct stat statbuf;
        // check if a directory /proc/xxxxx exists, where xxxxx is the pid.
        if (::stat(procname,&statbuf) == 0 && S_ISDIR(statbuf.st_mode))
            return pid;
    }
    // write current pid to file
    pid = getpid();
    sprintf(procname,"%d\n",pid);
    if (::lseek(fd,0,0) < 0 || ::write(fd,procname,strlen(procname)) < 0)
        throw IOException(pidFile,"write",errno);

    _pidFile = pidFile;
    ::atexit(removePidFile);
    return 0;
}

/* static */
void Process::removePidFile()
{
    // ignore errors
    if (_pidFile.length() > 0) ::unlink(_pidFile.c_str());
}
