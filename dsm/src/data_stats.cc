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
};

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

    map<dsm_sample_id_t,size_t> nsamps;

    map<dsm_sample_id_t,size_t> minlens;

    map<dsm_sample_id_t,size_t> maxlens;

    map<dsm_sample_id_t,int> minDeltaTs;

    map<dsm_sample_id_t,int> maxDeltaTs;
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
    if (t1i == t1s.end()) {
	t1s.insert(
	    make_pair<dsm_sample_id_t,dsm_time_t>(sampid,sampt));
	minDeltaTs[sampid] = INT_MAX;
    }
    else {
        int deltaT = (sampt - t2s[sampid] + USECS_PER_MSEC/2) / USECS_PER_MSEC;
	minDeltaTs[sampid] = std::min(minDeltaTs[sampid],deltaT);
	maxDeltaTs[sampid] = std::max(maxDeltaTs[sampid],deltaT);
    }
    t2s[sampid] = sampt;
    nsamps[sampid]++;

    size_t slen = samp->getDataByteLength();
    size_t mlen;

    map<dsm_sample_id_t,size_t>::iterator li = minlens.find(sampid);
    if (li == minlens.end()) minlens[sampid] = slen;
    else {
	mlen = li->second;
	if (slen < mlen) minlens[sampid] = slen;
    }

    mlen = maxlens[sampid];
    if (slen > mlen) maxlens[sampid] = slen;

    // cerr << samp->getId() << " " << samp->getTimeTag() << endl;
    return true;
}

void CounterClient::printResults()
{
    size_t maxnamelen = 6;
    int lenpow[2] = {5,5};
    int dtlog10[2] = {7,7};
    set<dsm_sample_id_t>::iterator si;
    for (si = sampids.begin(); si != sampids.end(); ++si) {
	dsm_sample_id_t id = *si;
	const string& sname = sensorNames[id];
	if (sname.length() > maxnamelen) maxnamelen = sname.length();
	size_t m = minlens[id];
	if (m > 0) {
	    int p = (int)ceil(log10((double)m));
	    lenpow[0] = std::max(lenpow[0],p+1);
	}
	m = maxlens[id];
	if (m > 0) {
	    int p = (int)ceil(log10((double)m));
	    lenpow[1] = std::max(lenpow[1],p+1);
	}
	int dt = abs(minDeltaTs[id]);
	if (dt > 0 && dt < INT_MAX) {
	    int p = (int)ceil(log10((double)dt+1));
	    dtlog10[0] = std::max(dtlog10[0],p + 2);
	}
	dt = maxDeltaTs[id];
	if (dt > 0) {
	    int p = (int)ceil(log10((double)dt+1));
	    dtlog10[1] = std::max(dtlog10[1],p + 2);
	}
    }
        
    struct tm tm;
    char tstr[64];
    cout << left << setw(maxnamelen) << (maxnamelen > 0 ? "sensor" : "") <<
    	right <<
    	"  dsm sampid    nsamps |------- start -------|  |------ end -----|    rate" <<
		setw(dtlog10[0] + dtlog10[1]) << " minMaxDT(sec)" <<
		setw(lenpow[0] + lenpow[1]) << " minMaxLen" <<
		endl;
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
        cout << left << setw(maxnamelen) << sensorNames[id] << right << ' ' <<
	    setw(4) << GET_DSM_ID(id) << ' ' <<
	    setw(5) << GET_SHORT_ID(id) << ' ' <<
	    ' ' << setw(9) << nsamps[id] << ' ' <<
	    t1str << "  " << t2str << ' ' << 
	    fixed << setw(7) << setprecision(2) <<
	    double(nsamps[id]-1) / (double(t2s[id]-t1s[id]) / USECS_PER_SEC) <<
	    setw(dtlog10[0]) << setprecision(3) <<
	    (minDeltaTs[id] < INT_MAX ? (float)minDeltaTs[id] / MSECS_PER_SEC : 0) <<
	    setw(dtlog10[1]) << setprecision(3) <<
	    (float)maxDeltaTs[id] / MSECS_PER_SEC <<
	    setw(lenpow[0]) << minlens[id] << setw(lenpow[1]) << maxlens[id] <<
	    endl;
    }
}

class DataStats
{
public:
    DataStats();

    ~DataStats() {}

    int run() throw();

    int parseRunstring(int argc, char** argv);

    static int main(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

private:
    static bool interrupted;

    bool processData;

    string xmlFileName;

    list<string> dataFileNames;

    string hostName;

    int port;

};

bool DataStats::interrupted = false;

void DataStats::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            DataStats::interrupted = true;
    break;
    }
}

void DataStats::setupSignals()
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
    act.sa_sigaction = DataStats::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

DataStats::DataStats(): processData(false),port(30000)
{
}

int DataStats::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
										
    while ((opt_char = getopt(argc, argv, "px:")) != -1) {
	switch (opt_char) {
	case 'p':
	    processData = true;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    for (; optind < argc; optind++) {
	string url(argv[optind]);
	if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	    url = url.substr(5);
	    size_t ic = url.find(':');
	    hostName = url.substr(0,ic);
	    if (ic < string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    return usage(argv[0]);
		}
	    }
	}
	else if (url.length() > 5 && !url.compare(0,5,"file:")) {
	    url = url.substr(5);
	    dataFileNames.push_back(url);
	}
	else dataFileNames.push_back(url);
    }
    if (dataFileNames.size() == 0 && hostName.length() == 0) return usage(argv[0]);

    return 0;
}

int DataStats::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p] [-x xml_file] inputURL ...\n\
    -p: process (optional). Pass samples to sensor process method\n\
    -x xml_file (optional), default: \n\
	 $ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
	 where <project>, <aircraft> and <flight> are read from the input data header\n\
    inputURL: data input (required). Either \"sock:host:port\" or\n\
	 one or more \"file:file_path\" or simply one or more file paths.\n\
Examples:\n" <<
    argv0 << " xxx.dat yyy.dat\n" <<
    argv0 << " file:/tmp/xxx.dat file:/tmp/yyy.dat\n" <<
    argv0 << " -p -x ads3.xml sock:hyper:30000\n" << endl;
    return 1;
}

int DataStats::main(int argc, char** argv)
{
    DataStats stats;

    int result;
    if ((result = stats.parseRunstring(argc,argv))) return result;

    setupSignals();

    return stats.run();
}

int DataStats::run() throw()
{

    int result = 0;
    CounterClient* counter = 0;

    try {
	dsm::IOChannel* iochan;

	if (dataFileNames.size() > 0) {

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
	    list<string>::const_iterator fi;
	    for (fi = dataFileNames.begin(); fi != dataFileNames.end(); ++fi)
		fset->addFileName(*fi);
#endif

	}
	else {
	    atdUtil::Socket* sock = new atdUtil::Socket(hostName,port);
	    iochan = new dsm::Socket(sock);
	}
	RawSampleInputStream sis(iochan);
	sis.init();
	sis.readHeader();

	SampleInputHeader header = sis.getHeader();

	auto_ptr<Project> project;
	list<DSMSensor*> allsensors;

	if (xmlFileName.length() == 0) {
	    if (getenv("ISFF") != 0)
		xmlFileName = Project::getConfigName("$ISFF",
		    "projects", header.getProjectName(),
		    header.getSiteName(),"ops",
		    header.getObsPeriodName(),"ads3.xml");
	    else
		xmlFileName = Project::getConfigName("$ADS3_CONFIG",
		    "projects", header.getProjectName(),
		header.getSiteName(),"flights",
		header.getObsPeriodName(),"ads3.xml");
	}

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(),&statbuf) == 0 || processData) {

	    auto_ptr<xercesc::DOMDocument> doc(
		    DSMEngine::parseXMLConfigFile(xmlFileName));

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

	counter = new CounterClient(allsensors);

	if (processData) {
	    list<DSMSensor*>::const_iterator si;
	    for (si = allsensors.begin(); si != allsensors.end(); ++si) {
		DSMSensor* sensor = *si;
		sensor->init();
		sis.addProcessedSampleClient(counter,sensor);
	    }
	}
	else sis.addSampleClient(counter);


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
	result = 1;
    }
    catch (atdUtil::Exception& ioe) {
        cerr << ioe.what() << endl;
	result = 1;
    }

    if (counter) counter->printResults();
    delete counter;

    return result;
}

int main(int argc, char** argv)
{
    return DataStats::main(argc,argv);
}
