
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
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <fcntl.h>
#include <cassert>
#include <cstring>
#include <sstream>

#include <iostream>
#include <iomanip>
#include <cstdlib>  // atexit()

using namespace std;
using namespace nidas::util;

/* static */
string Process::_pidFile;

/* static */
map<string,char*> Process::_environment;

/* static */
Mutex Process::_envLock;

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

Process::Process(pid_t pid): _pid(pid),
    _infd(-1),_instream_ap(new ostream(0)),
    _outfd(-1),_errfd(-1)
{
    // check if process exists
    if (::kill(_pid,0) < 0) _pid = -1;
}

Process::Process(): _pid(-1),_infd(-1),
    _outfd(-1),_errfd(-1)
{
}

Process::Process(const Process& x):
    _pid(x._pid),
    _infd(x._infd),_inbuf_ap(x._inbuf_ap),_instream_ap(x._instream_ap),
    _outfd(x._outfd),_outbuf_ap(x._outbuf_ap),_outstream_ap(x._outstream_ap),
    _errfd(x._errfd),_errbuf_ap(x._errbuf_ap),_errstream_ap(x._errstream_ap)
{
    x._infd = -1;
    x._outfd = -1;
    x._errfd = -1;
}

Process& Process::operator = (const Process& x)
{
    if (this != &x) {
        _pid = x._pid;
        _infd = x._infd;
        _inbuf_ap = x._inbuf_ap;
        _instream_ap = x._instream_ap;

        _outfd = x._outfd;
        _outbuf_ap = x._outbuf_ap;
        _outstream_ap = x._outstream_ap;

        _errfd = x._errfd;
        _errbuf_ap = x._errbuf_ap;
        _errstream_ap = x._errstream_ap;
    }
    return *this;
}

Process::~Process()
{
}

void Process::setInFd(int val)
{
    _infd = val;
    _inbuf_ap.reset(
        new __gnu_cxx::stdio_filebuf<char>(_infd,ios_base::out,1024));
    _instream_ap.reset(new ostream(_inbuf_ap.get()));
}

void Process::setOutFd(int val)
{
    _outfd = val;
    _outbuf_ap.reset(
        new __gnu_cxx::stdio_filebuf<char>(_outfd,ios_base::in,1024));
    _outstream_ap.reset(new istream(_outbuf_ap.get()));
}

void Process::setErrFd(int val)
{
    _errfd = val;
    _errbuf_ap.reset(
        new __gnu_cxx::stdio_filebuf<char>(_errfd,ios_base::in,0));
    _errstream_ap.reset(new istream(_errbuf_ap.get()));
}

void Process::closeIn()
{
    _instream_ap.reset(0);
    _inbuf_ap.reset(0);
    if (_infd >= 0) ::close(_infd);
    _infd = -1;
}

void Process::closeOut()
{
    _outstream_ap.reset(0);
    _outbuf_ap.reset(0);
    if (_outfd >= 0) ::close(_outfd);
    _outfd = -1;
}

void Process::closeErr()
{
    _errstream_ap.reset(0);
    _errbuf_ap.reset(0);
    if (_errfd >= 0) ::close(_errfd);
    _errfd = -1;
}

/* static */
Process Process::spawn(const string& cmd,
    const vector<string>& args,
    const map<string,string>& env,
    int niceval) throw(IOException)
{
    int pid;
    int infd[2];
    int outfd[2];
    int errfd[2];

    if (pipe(infd) < 0)
        throw IOException(cmd,"stdin pipe",errno);
    if (pipe(outfd) < 0)
        throw IOException(cmd,"stdout pipe",errno);
    if (pipe(errfd) < 0)
        throw IOException(cmd,"stderr pipe",errno);

    switch (pid=fork()) {
    case 0: // child
    {
        ::close(0);
        dup(infd[0]);

        ::close(1);
        dup(outfd[1]);

        ::close(2);
        dup(errfd[1]);

        /* Close other open file descriptors */
        unsigned int i;
        struct rlimit rl;
        rl.rlim_max = 0;
        getrlimit(RLIMIT_NOFILE, &rl);
        for (i = 3; i < rl.rlim_max; i++) (void) ::close(i);

        // since we're overwriting this process we don't need to delete [] this.
        const char **newargs = new const char*[args.size()+1];

        for (i = 0; i < args.size(); i++)
          newargs[i] = ::strdup(args[i].c_str());
        newargs[i] = 0;

        /* There isn't an execvpe where one can pass env variables,
         * so we do a putenvs before the exec.
         */
        if (env.size() > 0) {
            // since we're overwriting this process we don't need to delete [] this.
            char **newenv = new char*[env.size()];
            map<string,string>::const_iterator mi = env.begin();
            for (i = 0 ; mi != env.end(); ++mi,i++) {
                const string& name = mi->first;
                const string& val = mi->second;
                string envvar = name + '=' + val;
                newenv[i] = strdup(envvar.c_str());
                putenv(newenv[i]);
            }
        }
        nice(niceval);

        execvp(cmd.c_str(),(char *const *)newargs); // shouldn't return

        // program not found. Send a message on stderr and exit(127).
        // An Exception can't be caught here.
        perror(cmd.c_str());
        _exit(127);     // convention for command not found. Closes open fds.
    }
    case -1:
        throw IOException(cmd,"fork",errno);
    default:      // parent
        break;

    }

    ::close(infd[0]);
    ::close(outfd[1]);
    ::close(errfd[1]);

    Process proc(pid);
    proc.setInFd(infd[1]);
    proc.setOutFd(outfd[0]);
    proc.setErrFd(errfd[0]);
    return proc;
}

/* static */
Process Process::spawn(const string& cmd,
    const vector<string>& args,
    int niceval) throw(IOException)
{
    map<string,string> env = map<string,string>();
    return spawn(cmd,args,env,niceval);
}

/* static */
Process Process::spawn(const string& cmd) throw(IOException)
{
    int pid;
    int infd[2];
    int outfd[2];
    int errfd[2];

    if (pipe(infd) < 0)
        throw IOException(cmd,"stdin pipe",errno);
    if (pipe(outfd) < 0)
        throw IOException(cmd,"stdout pipe",errno);
    if (pipe(errfd) < 0)
        throw IOException(cmd,"stderr pipe",errno);

    switch (pid=fork()) {
    case 0: // child
    {
        ::close(0);
        dup(infd[0]);

        ::close(1);
        dup(outfd[1]);

        ::close(2);
        dup(errfd[1]);

        /* Close other open file descriptors */
        unsigned int i;
        struct rlimit rl;
        rl.rlim_max = 0;
        getrlimit(RLIMIT_NOFILE, &rl);
        for (i = 3; i < rl.rlim_max; i++) (void) ::close(i);

        const char **newargs = new const char*[4];

        newargs[0] = "sh";
        newargs[1] = "-c";
        newargs[2] = cmd.c_str();
        newargs[3] = 0;

        execvp("sh",(char *const *)newargs);

        // program not found. Send a message on stderr and exit(127).
        // An Exception can't be caught here.
        perror(cmd.c_str());
        _exit(127);     // convention for command not found. Closes open fds.
    }
    case -1:
        throw IOException(cmd,"fork",errno);
    default:      // parent
        break;
    }
    ::close(infd[0]);
    ::close(outfd[1]);
    ::close(errfd[1]);

    Process proc(pid);
    proc.setInFd(infd[1]);
    proc.setOutFd(outfd[0]);
    proc.setErrFd(errfd[0]);
    return proc;
}

void Process::kill(int sig) throw(IOException)
{
    // ESRCH The  pid or process group does not exist.  Note that an existing
    //   process might be a zombie, a  process  which  already  committed
    //   termination, but has not yet been wait(2)ed for.
    if (_pid > 0 && ::kill(_pid,sig) < 0) {
        ostringstream pname;
        pname << "process id " << _pid;
        throw IOException(pname.str(),"kill",errno);
    }
}

int Process::wait(bool hang,int* status) throw(IOException)
{

    if (_pid <= 0) return -1;
    int res;
    int options = 0;    // WNOHANG, WUNTRACED,WCONTINUED
    if (!hang) options |= WNOHANG;
    if ((res = ::waitpid(_pid,status,options)) < 0) {
        ostringstream pname;
        pname << "process id " << _pid;
        throw IOException(pname.str(),"wait",errno);
    }
    if (res > 0 && WIFEXITED(*status)) {
        _pid = -1;
        closeIn();
        closeOut();
        closeErr();
    }
    return res;
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

/* static */
void Process::addEffectiveCapability(int cap) throw(Exception)
{
#ifdef HAS_CAPABILITY_H 

// #define USE_CAPSET
#ifdef USE_CAPSET
    /*
     * using capset with _LINUX_CAPABILITY_VERSION_1, causes setuid(2423) from root to fail,
     * permission denied, with selinux enabled, 2.6.27.12-170.2.5.fc10 on porter.
     * Apparently user is not allowed to have that capability, or perhaps the
     * the old capset code is obsolete in this kernel.
     */
    struct __user_cap_header_struct cap_head;
    struct __user_cap_data_struct cap_data;

    // cap_user_header_t cap_head;
    // cap_user_data_t cap_data;
    unsigned int cap_mask = 0;

    cap_head.version = _LINUX_CAPABILITY_VERSION;
    cap_head.pid = 0;

    if (capget(&cap_head, &cap_data) < 0)
        throw IOException("Process","capget",errno);

    switch (cap_head.version) {
    case _LINUX_CAPABILITY_VERSION_1:
        cerr << "_LINUX_CAPABILITY_VERSION_1" << endl;
        break;
    case _LINUX_CAPABILITY_VERSION_2:
        cerr << "_LINUX_CAPABILITY_VERSION_2" << endl;
        break;
    case _LINUX_CAPABILITY_VERSION_3:
        cerr << "_LINUX_CAPABILITY_VERSION_3" << endl;
        break;
    default:
        cerr << "cap_head.version=" << hex << cap_head.version << dec << endl;
        break;
    }

    if (cap & CAP_SYS_NICE)
    {
      cap_mask |= (1 << CAP_SYS_NICE);
      // cap_mask |= (1 << CAP_SETPCAP);
    }

    cerr << "cap_mask=" << hex << cap_mask << dec << endl;
    // cap_data.effective = cap_data.inheritable = cap_data.permitted = cap_mask;
    cap_data.effective = cap_data.permitted = cap_mask;
    cap_data.inheritable = 0;

    if (capset(&cap_head, &cap_data) < 0)
        throw IOException("Process","capset",errno);

#else
    cap_t caps;
    cap_value_t cap_list[1];
    int nlist = 1;

    // cerr << "adding capability " << cap << endl;

    caps = cap_get_proc();
    if (caps == NULL) throw IOException("Process","cap_get_proc",errno);

    cap_list[0] = cap;

    /*
    if (cap_set_flag(caps, CAP_INHERITABLE, nlist, cap_list, CAP_SET) == -1)
        throw IOException("Process","cap_set_flag",errno);
    */

    /*
    if (cap_set_flag(caps, CAP_PERMITTED, nlist, cap_list, CAP_SET) == -1)
        throw IOException("Process","cap_set_flag",errno);
    */

    if (cap_set_flag(caps, CAP_EFFECTIVE, nlist, cap_list, CAP_SET) == -1)
        throw IOException("Process","cap_set_flag",errno);

    if (cap_set_proc(caps) == -1)
        throw IOException("Process","cap_set_proc",errno);

    if (cap_free(caps) == -1)
        throw IOException("Process","cap_free",errno);
    // cerr << "added capability " << cap << endl;

#endif

#ifdef PR_GET_SECUREBITS

/* These defs are missing on porter, fedora 10. Googling
 * found some example code to figure out what the values should be. */
#ifndef SECURE_NO_SETUID_FIXUP
#define SECURE_NO_SETUID_FIXUP         2
#define SECURE_NO_SETUID_FIXUP_LOCKED  3  /* make bit-2 immutable */ 
#define SECURE_KEEP_CAPS               4
#define SECURE_KEEP_CAPS_LOCKED        5  /* make bit-4 immutable */ 
#endif

    // cerr << "doing prctl(PR_SET_SECUREBITS,SECURE_KEEP_CAPS)" << endl;
    int bits = prctl(PR_GET_SECUREBITS);
    // cerr << "PR_GET_SECUREBITS=" << hex << bits << dec << endl;
    if (bits < 0) throw IOException("Process","prctl(PR_GET_SECUREBITS)",errno);
    if (!(bits & (1 << SECURE_NO_SETUID_FIXUP)) &&
        prctl(PR_SET_SECUREBITS, 1 << SECURE_NO_SETUID_FIXUP) < 0)
        throw IOException("Process","prctl(PR_SET_SECUREBITS,1<<SECURE_NO_SETUID_FIXUP)",errno);

#else
    // didn't work in fedora10
    cerr << "doing prctl PR_SET_KEEPCAPS" << endl;
    if (prctl(PR_SET_KEEPCAPS,1) < 0)
        throw IOException("Process","prctl(PR_SET_KEEPCAPS,1)",errno);
#endif

#endif
}

/* static */
bool Process::getEffectiveCapability(int cap) throw(Exception)
{
#ifdef HAS_CAPABILITY_H 
// #define USE_CAPGET
#ifdef USE_CAPGET
    struct __user_cap_header_struct cap_head;
    struct __user_cap_data_struct cap_data;

    cap_head.version = _LINUX_CAPABILITY_VERSION;
    cap_head.pid = 0;

    if (capget(&cap_head, &cap_data) < 0)
        throw IOException("Process","capset",errno);

    switch (cap_head.version) {
    case _LINUX_CAPABILITY_VERSION_1:
        cerr << "_LINUX_CAPABILITY_VERSION_1" << endl;
        break;
    case _LINUX_CAPABILITY_VERSION_2:
        cerr << "_LINUX_CAPABILITY_VERSION_2" << endl;
        break;
    case _LINUX_CAPABILITY_VERSION_3:
        cerr << "_LINUX_CAPABILITY_VERSION_3" << endl;
        break;
    default:
        cerr << "cap_head.version=" << hex << cap_head.version << dec << endl;
        break;
    }

    if ( cap_data.effective & (1 << CAP_SYS_NICE)) return true;
    return false;

#else

    cap_t caps;
    cap_flag_value_t result = CAP_CLEAR;

    // cerr << "getting capability " << cap << endl;

    caps = cap_get_proc();
    if (caps == NULL) throw IOException("Process","cap_get_proc",errno);

    if (cap_get_flag(caps, cap, CAP_EFFECTIVE, &result) == -1)
        throw IOException("Process","cap_set_flag",errno);

    if (cap_free(caps) == -1)
        throw IOException("Process","cap_free",errno);
    // cerr << "got capability " << cap << " result=" << result << endl;

    return result == CAP_SET;
#endif

#else
    return false;
#endif
}

/* static */
string Process::expandEnvVars(string input)
{
    string::size_type dollar;

    string result;
    bool substitute = true;

    for (;;) {
        string::size_type lastpos = 0;
        substitute = false;

        while ((dollar = input.find('$',lastpos)) != string::npos) {

            result.append(input.substr(lastpos,dollar-lastpos));
            lastpos = dollar;

            string::size_type openparen = input.find('{',dollar);
            string::size_type tokenStart;
            int tokenLen= 0;
            int totalLen;

            if (openparen == dollar + 1) {
                string::size_type closeparen = input.find('}',openparen);
                if (closeparen == string::npos) break;
                tokenStart = openparen + 1;
                tokenLen= closeparen - openparen - 1;
                totalLen = closeparen - dollar + 1;
                lastpos = closeparen + 1;
            }
            else {
                string::size_type endtok = input.find_first_of("/.$",dollar + 1);
                if (endtok == string::npos) endtok = input.length();
                tokenStart = dollar + 1;
                tokenLen= endtok - dollar - 1;
                totalLen = endtok - dollar;
                lastpos = endtok;
            }
            string value;
            if (tokenLen > 0 && getEnvVar(input.substr(tokenStart,tokenLen),value)) {
                substitute = true;
                result.append(value);
            }
            else result.append(input.substr(dollar,totalLen));
        }

        result.append(input.substr(lastpos));
        if (!substitute) break;
        input = result;
        result.clear();
    }
    // cerr << "input: \"" << input << "\" expanded to \"" <<
    // 	result << "\"" << endl;
    return result;
}

/* static */
bool Process::getEnvVar(const string& token, string& value)
{
    const char* val = ::getenv(token.c_str());
    if (val) value = val;
    return val != 0;
}

/* static */
void Process::setEnvVar(const string& name, const string& value)
{

    Autolock autolock(_envLock);

    char* curval = 0;
    map<string,char*>::const_iterator ei = _environment.find(name);
    if (ei != _environment.end()) curval = ei->second;

    string newstr;
    if (value.length() > 0) newstr = name + "=" + value;
    else string newstr = name;

    char* newval = new char[newstr.length() + 1];
    strcpy(newval,newstr.c_str());
    ::putenv(newval);
    delete [] curval;
    _environment.insert(make_pair(name,newval));
}

