/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <FileSet.h>
#include <Socket.h>
#include <RawSampleInputStream.h>
// #include <Sample.h>
#include <dsm_sample.h>
#include <DSMEngine.h>
#include <atdUtil/EOFException.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);
    static void usage(const char* argv0);
    bool process;
    string xmlFileName;
    string dataFileName;
    string hostName;
    int port;
};

Runstring::Runstring(int argc, char** argv): process(false),port(50000)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
										
    while ((opt_char = getopt(argc, argv, "px:")) != -1) {
	switch (opt_char) {
	case 'p':
	    process = true;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind == argc - 1) {
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
    if (dataFileName.length() == 0 && hostName.length() == 0) usage(argv[0]);

    if (process && xmlFileName.length() == 0) usage(argv[0]);
    if (optind != argc) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p] [-x xml_file] URL\n\
  -p: process (optional). Pass samples to sensor process method\n\
  -x xml_file (optional). Name of XML file (required with -p option)\n\
  URL (required). Either \"file:file_path\", \"sock:host:port\",\n\
      or simply a file_path.\n\
Examples:\n" <<
	argv0 << " /tmp/xxx.dat\n" <<
	argv0 << " file:/tmp/xxx.dat\n" <<
	argv0 << " -p -x ads3.xml sock:hyper:10000\n" << endl;
    exit(1);
}

class CounterClient: public SampleClient 
{
public:

    CounterClient(const list<DSMSensor*>& sensors);

    virtual ~CounterClient() {}

    bool receive(const Sample* samp) throw();

    void printResults();


private:
    map<dsm_sample_id_t,string> sensorNames;

    set<dsm_sample_id_t> sampids;
    map<dsm_sample_id_t,dsm_time_t> t1s;
    map<dsm_sample_id_t,dsm_time_t> t2s;
    map<dsm_sample_id_t,unsigned long> nsamps;
};

CounterClient::CounterClient(const list<DSMSensor*>& sensors)
{
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;
	sensorNames[sensor->getId()] =
	    sensor->getDSMConfig()->getName() + ":" + sensor->getDeviceName();

	// for samples show the first variable name, followed by ",..."
	// if more than one.
	const vector<const SampleTag*>& stags = sensor->getSampleTags();
	vector<const SampleTag*>::const_iterator ti;
	for (ti = stags.begin(); ti != stags.end(); ++ti) {
	    const SampleTag* stag = *ti;
	    if (stag->getVariables().size() > 0) {
		string varname = stag->getVariables().front()->getName();
		if (stag->getVariables().size() > 1) varname += ",...";
		sensorNames[stag->getId()] = varname;
	    }
	}
    }
}

bool CounterClient::receive(const Sample* samp) throw()
{
    dsm_time_t sampt = samp->getTimeTag();

    dsm_sample_id_t sampid = samp->getId();
    sampids.insert(sampid);

    map<dsm_sample_id_t,dsm_time_t>::iterator t1i =
	t1s.find(sampid);
    if (t1i == t1s.end())
	t1s.insert(
	    make_pair<dsm_sample_id_t,dsm_time_t>(sampid,sampt));
    t2s[sampid] = sampt;
    nsamps[sampid]++;
    // cerr << samp->getId() << " " << samp->getTimeTag() << endl;
    return true;
}

void CounterClient::printResults()
{
    size_t maxlen = 6;
    set<dsm_sample_id_t>::iterator si;
    for (si = sampids.begin(); si != sampids.end(); ++si) {
	dsm_sample_id_t id = *si;
	const string& sname = sensorNames[id];
	if (sname.length() > maxlen) maxlen = sname.length();
    }
        
    struct tm tm;
    char tstr[64];
    cout << left << setw(maxlen) << (maxlen > 0 ? "sensor" : "") << right <<
    	"  dsm sampid    nsamps |------- start -------|  |------ end -----|    rate" << endl;
    for (si = sampids.begin(); si != sampids.end(); ++si) {
	dsm_sample_id_t id = *si;
	time_t ut = t1s[id] / USECS_PER_SEC;
	gmtime_r(&ut,&tm);
	strftime(tstr,sizeof(tstr),"%Y %m %d %H:%M:%S",&tm);
	int msec = (int)(t1s[id] % USECS_PER_SEC) / USECS_PER_MSEC;
	sprintf(tstr + strlen(tstr),".%03d",msec);
	string t1str(tstr);
	ut = t2s[id] / USECS_PER_SEC;
	gmtime_r(&ut,&tm);
	strftime(tstr,sizeof(tstr),"%m %d %H:%M:%S",&tm);
	msec = (int)(t2s[id] % USECS_PER_SEC) / USECS_PER_MSEC;
	sprintf(tstr + strlen(tstr),".%03d",msec);
	string t2str(tstr);
        cout << left << setw(maxlen) << sensorNames[id] << right << ' ' <<
	    setw(4) << GET_DSM_ID(id) << ' ' <<
	    setw(5) << GET_SHORT_ID(id) << ' ' <<
	    ' ' << setw(9) << nsamps[id] << ' ' <<
	    t1str << "  " << t2str << ' ' << 
	    fixed << setw(7) << setprecision(2) <<
	    float(nsamps[id]) / ((t2s[id]-t1s[id]) / USECS_PER_SEC) << endl;
    }
}

class FileStats
{
public:
    FileStats() {}
    ~FileStats() {}

    static int main(int argc, char** argv);

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static bool interrupted;
};

bool FileStats::interrupted = false;

void FileStats::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            FileStats::interrupted = true;
    break;
    }
}

void FileStats::setupSignals()
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
    act.sa_sigaction = FileStats::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

int FileStats::main(int argc, char** argv)
{
    Runstring rstr(argc,argv);
    dsm::IOChannel* iochan;
    setupSignals();

    if (rstr.dataFileName.length() > 0) {

	FileSet* fset = new dsm::FileSet();
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
    RawSampleInputStream sis(iochan);
    sis.init();

    auto_ptr<Project> project;
    list<DSMSensor*> allsensors;

    if (rstr.xmlFileName.length() > 0) {
	auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(rstr.xmlFileName));

        project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	const list<Site*>& sitelist = project->getSites();
	list<Site*>::const_iterator ai;
	for (ai = sitelist.begin(); ai != sitelist.end(); ++ai) {
	    Site* site = *ai;
	    const list<DSMConfig*>& dsms = site->getDSMConfigs();
	    list<DSMConfig*>::const_iterator di;
	    for (di = dsms.begin(); di != dsms.end(); ++di) {
		DSMConfig* dsm = *di;
		const list<DSMSensor*>& sensors = dsm->getSensors();
		allsensors.insert(allsensors.end(),sensors.begin(),sensors.end());
	    }
	}
    }

    CounterClient counter(allsensors);

    if (rstr.process) {
	list<DSMSensor*>::const_iterator si;
	for (si = allsensors.begin(); si != allsensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->init();
	    sis.addProcessedSampleClient(&counter,sensor);
	}
    }
    else sis.addSampleClient(&counter);


    try {
	for (;;) {
	    sis.readSamples();
	    if (interrupted) break;
	}
    }
    catch (atdUtil::EOFException& eof) {
        cerr << eof.what() << endl;
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
    }

    counter.printResults();
    return 0;
}

int main(int argc, char** argv)
{
    return FileStats::main(argc,argv);
}
