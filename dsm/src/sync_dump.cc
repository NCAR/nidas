/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-06-10 14:01:31 -0600 (Fri, 10 Jun 2005) $

    $LastChangedRevision: 2287 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/hiaper/ads3/dsm/src/file_dump.cc $
 ********************************************************************

*/

#include <time.h>

#include <FileSet.h>
#include <Socket.h>
#include <SampleInput.h>
#include <SyncRecordReader.h>

#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace dsm;
using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);

    static void usage(const char* argv0);
    string dataFileName;
    string hostName;
    int port;

    string varname;
};

Runstring::Runstring(int argc, char** argv):port(0)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "")) != -1) {
	switch (opt_char) {
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind != argc - 2) usage(argv[0]);

    varname = string(argv[optind++]);

    string url(argv[optind++]);
    if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	url = url.substr(5);
	size_t ic = url.find(':');
	if (ic == string::npos) {
	    cerr << "Invalid host:port parameter: " << url << endl;
	    usage(argv[0]);
	}
	hostName = url.substr(0,ic);
	istringstream ist(url.substr(ic+1));
	ist >> port;
	if (ist.fail()) {
	    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
	    usage(argv[0]);
	}
    }
    else if (url.length() > 5 && !url.compare(0,5,"file:")) {
	url = url.substr(5);
	dataFileName = url;
    }
    else dataFileName = url;
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " variable URL\n\
  var: a variable name\n\
  URL: Either \"file:file_path\", \"sock:host:port\",\n\
      or simply a file_path.\n\
Examples:\n" <<
	argv0 << " DPRES /tmp/xxx.dat\n" <<
	argv0 << " DPRES file:/tmp/xxx.dat\n" <<
	argv0 << " DPRES sock:hyper:10000\n" << endl;
    exit(1);
}


class SyncDumper
{
public:

    SyncDumper();

    ~SyncDumper();

public:

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv);

    static bool interrupted;

    void printHeader();

};

SyncDumper::SyncDumper()
{
}

void SyncDumper::printHeader()
{
    cout << "|--- date time -------|  bytes" << endl;
}

bool SyncDumper::interrupted = false;

void SyncDumper::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SyncDumper::interrupted = true;
    break;
    }
}

void SyncDumper::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                                                
    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = SyncDumper::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}
int SyncDumper::main(int argc, char** argv)
{

    Runstring rstr(argc,argv);

    setupSignals();

    IOChannel* iochan = 0;

    if (rstr.dataFileName.length() > 0) {
	dsm::FileSet* fset = new dsm::FileSet();
	iochan = fset;

#ifdef USE_FILESET_TIME_CAPABILITY
	struct tm tm;
	strptime("2005 04 05 00:00:00","%Y %m %d %H:%M:%S",&tm);
	time_t start = timegm(&tm);

	strptime("2005 04 06 00:00:00","%Y %m %d %H:%M:%S",&tm);
	time_t end = timegm(&tm);

	fset->setDir("/tmp/RICO/hiaper");
	fset->setFileName("radome_%Y%m%d_%H%M%S.dat");
	fset->setStartTime(start);
	fset->setEndTime(end);
#else
	fset->setFileName(rstr.dataFileName);
#endif
    }
    else {
	atdUtil::Socket* sock = new atdUtil::Socket(rstr.hostName,rstr.port);
	iochan = new dsm::Socket(sock);
    }

    SyncRecordReader reader(iochan);

    size_t numFloats = reader.getNumFloats();
    cerr << "numFloats=" << reader.getNumFloats() << endl;

    const list<const SyncRecordVariable*>& vars = reader.getVariables();
    cerr << "num of variables=" << vars.size() << endl;

#ifdef USE_FIND_IF
    // predicate class which compares variable names against a string.
    class MatchName {
    public:
        MatchName(const string& name): _name(name) {}
	bool operator()(const SyncRecordVariable* v) const {
	    return _name.compare(v->getName()) == 0;
	}
    private:
	string _name;
    } matcher(rstr.varname);

    list<const SyncRecordVariable*>::const_iterator vi =
	std::find_if(vars.begin(), vars.end(), matcher);
#else
    list<const SyncRecordVariable*>::const_iterator vi;
    for (vi = vars.begin(); vi != vars.end(); ++vi) {
        const SyncRecordVariable *var = *vi;
	if (!rstr.varname.compare(var->getName())) break;
    }
#endif

    if (vi == vars.end()) {
        cerr << "Can't find variable " << rstr.varname << endl;
	return 1;
    }

    const SyncRecordVariable* var = *vi;
    size_t varoffset = var->getSyncRecOffset();
    size_t lagoffset = lagoffset = var->getLagOffset();
    int irate = (int)ceil(var->getSampleRate());
    int deltatUsec = (int)rint(USECS_PER_SEC / var->getSampleRate());

    dsm_time_t tt;
    float* rec = new float[numFloats];
    struct tm tm;
    char cstr[64];

    try {
	for (;;) {
	    size_t len = reader.read(&tt,rec,numFloats);
	    if (interrupted) {
		reader.interrupt();
		continue;
	    }
	    if (len == 0) continue;

	    cerr << "lag= " << rec[lagoffset] << endl;
	    if (!isnan(rec[lagoffset])) tt += (int) rec[lagoffset];

	    for (int i = 0; i < irate; i++) {
		time_t ut = tt / USECS_PER_SEC;
		gmtime_r(&ut,&tm);
		int msec = (tt % USECS_PER_SEC) / USECS_PER_MSEC;
		strftime(cstr,sizeof(cstr),"%Y %m %d %H:%M:%S",&tm);
		cout << cstr << '.' << setw(3) << setfill('0') << msec << ' ' <<
		    rec[varoffset + i] << endl;
		tt += deltatUsec;
	    }
	    cout << endl;
	}
    }
    catch (const atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return SyncDumper::main(argc,argv);
}
