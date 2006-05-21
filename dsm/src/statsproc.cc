/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-08-29 15:10:54 -0600 (Mon, 29 Aug 2005) $

    $LastChangedRevision: 2753 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/src/data_dump.cc $
 ********************************************************************

*/

#include <time.h>

#include <Project.h>
#include <FileSet.h>
#include <Socket.h>
#include <SampleInput.h>
#include <StatisticsProcessor.h>
#include <AsciiOutput.h>
#include <XMLParser.h>
#include <atdUtil/Logger.h>

// #include <set>
// #include <map>
// #include <iostream>
// #include <iomanip>

using namespace dsm;
using namespace std;

class StatsProcess
{
public:

    StatsProcess();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

private:

    static bool interrupted;

    string xmlFileName;

    list<string> dataFileNames;

    string hostName;

    int port;

};

int main(int argc, char** argv)
{
    return StatsProcess::main(argc,argv);
}


/* static */
bool StatsProcess::interrupted = false;

/* static */
void StatsProcess::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            StatsProcess::interrupted = true;
    break;
    }
}

/* static */
void StatsProcess::setupSignals()
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
    act.sa_sigaction = StatsProcess::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int StatsProcess::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-x xml_file] [ raw_data_file ]...\n\
    -x xml_file (optional), default: \n\
	$ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
    or\n\
	$ISFF/projects/<project>/ISFF/ops/<ops>/ads3.xml\n\
	where <project>, <aircraft> and <flight> are read from the input data header\n\
    raw_data_file: names of one or more raw data files, separated by spaces\n\
" << endl;
    return 1;
}

/* static */
int StatsProcess::main(int argc, char** argv) throw()
{
    setupSignals();

    StatsProcess stats;

    int res;
    
    if ((res = stats.parseRunstring(argc,argv)) != 0) return res;

    if (stats.hostName.length() > 0) {
	// fork to background, send stdout/stderr to /dev/null
	if (0) {
	if (daemon(0,0) < 0) {
	    atdUtil::IOException e("DSMServer","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        atdUtil::Logger::createInstance("statsproc",LOG_CONS,LOG_LOCAL5);
	}
    }
    return stats.run();
}


StatsProcess::StatsProcess()
{
}

int StatsProcess::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "p:sx:")) != -1) {
	switch (opt_char) {
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    for ( ; optind < argc; optind++) {
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
    if (dataFileNames.size() == 0 && hostName.length() == 0)
    	return usage(argv[0]);
    return 0;
}

int StatsProcess::run() throw()
{

    IOChannel* iochan = 0;

    try {
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
	    atdUtil::Socket* sock = 0;
	    for (int i = 0; !sock && !interrupted; i++) {
		try {
		    sock = new atdUtil::Socket(hostName,port);
		}
		catch(const atdUtil::IOException& e) {
		    if (i > 2)
		    	atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    	"%s: retrying",e.what());
		    sleep(10);
		}
	    }
	    iochan = new dsm::Socket(sock);
	}

	// SortedSampleStream owns the iochan ptr.
	SortedSampleInputStream input(iochan);
	input.setSorterLengthMsecs(2000);

	cerr << "input, #sampleTags=" << input.getSampleTags().size() << endl;

	// Block while waiting for heapSize to become less than heapMax.
	input.setHeapBlock(true);
	input.setHeapMax(10000000);
	input.init();

	input.readHeader();
	SampleInputHeader header = input.getHeader();

	string systemName = header.getSystemName();

	if (xmlFileName.length() == 0) {
	    if (systemName == "ISFF" && getenv("ISFF") != 0)
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

	auto_ptr<Project> project;
	XMLParser parser;

	cerr << "parsing: " << xmlFileName << endl;
	auto_ptr<xercesc::DOMDocument> doc(parser.parse(xmlFileName));

	project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	// Find a server with a StatisticsProcessor
	DSMServer* server = 0;
	StatisticsProcessor* sproc = 0;
	DSMServerIterator sitr = project->getDSMServerIterator();
	for ( ; sitr.hasNext(); ) {
	    server = sitr.next();

	    ProcessorIterator pitr = server->getProcessorIterator();
	    for ( ; pitr.hasNext(); ) {
		SampleIOProcessor* proc = pitr.next();
		sproc = dynamic_cast<StatisticsProcessor*>(proc);
		if (sproc) break;
	    }
	    if (sproc) break;
	}
	if (!sproc) {
	    cerr << "No StatisticsProcessor found" << endl;
	    return 1;
	}

	SensorIterator si = server->getSensorIterator();
	for (; si.hasNext(); ) {
	    DSMSensor* sensor = si.next();
	    sensor->init();
	}

	SampleTagIterator ti = server->getSampleTagIterator();
	for ( ; ti.hasNext(); ) {
	    const SampleTag* stag = ti.next();
	    input.addSampleTag(stag);
	}

	sproc->connect(&input);

	// AsciiOutput* output = new AsciiOutput();
	// sproc->connected(output);
	// output->connect();

	try {
	    for (;;) {
		if (interrupted) break;
		input.readSamples();
	    }
	}
	catch (atdUtil::EOFException& e) {
	    cerr << "EOF received: flushing buffers" << endl;
	    input.flush();
	    sproc->disconnect(&input);

	    input.close();
	    // sproc->disconnected(output);
	    throw e;
	}
	catch (atdUtil::IOException& e) {
	    input.close();
	    sproc->disconnect(&input);
	    // sproc->disconnected(output);
	    throw e;
	}
    }
    catch (atdUtil::EOFException& eof) {
        cerr << eof.what() << endl;
	return 1;
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (atdUtil::InvalidParameterException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (dsm::XMLException& e) {
        cerr << e.what() << endl;
	return 1;
    }
    catch (atdUtil::Exception& e) {
        cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

