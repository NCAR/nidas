/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-08-29 15:10:54 -0600 (Mon, 29 Aug 2005) $

    $LastChangedRevision: 2753 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/src/data_dump.cc $
 ********************************************************************

*/

#include <ctime>

#include <nidas/core/Project.h>
#include <nidas/dynld/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/StatisticsProcessor.h>
#include <nidas/dynld/AsciiOutput.h>
#include <nidas/core/XMLParser.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

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

    int sorterLength;

    bool daemonMode;

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
Usage: " << argv0 << " [-x xml_file] [-z] input ...\n\
    -s sorterLength: input data sorter length in milliseconds\n\
    -x xml_file: (optional), the default value is read from the input\n\
    -z: run in daemon mode (in the background, log messages to syslog)\n\
    input: names of one or more raw data files, or sock:hostname\n\
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

    if (stats.daemonMode && stats.hostName.length() > 0) {
	// fork to background, send stdout/stderr to /dev/null
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMServer","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        n_u::Logger::createInstance("statsproc",LOG_CONS,LOG_LOCAL5);
    }
    return stats.run();
}


StatsProcess::StatsProcess(): port(30000),sorterLength(1000),daemonMode(false)
{
}

int StatsProcess::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "s:x:z")) != -1) {
	switch (opt_char) {
	case 's':
	    {
	        istringstream ist(optarg);
		ist >> sorterLength;
		if (ist.fail() || sorterLength < 0 || sorterLength > 10000) {
                    cerr << "Invalid sorter length: " << optarg << endl;
                    return usage(argv[0]);
		}
	    }
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case 'z':
	    daemonMode = true;
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
	    FileSet* fset = new nidas::dynld::FileSet();
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
	    n_u::Socket* sock = 0;
	    for (int i = 0; !sock && !interrupted; i++) {
		try {
		    sock = new n_u::Socket(hostName,port);
		}
		catch(const n_u::IOException& e) {
		    if (i > 2)
		    	n_u::Logger::getInstance()->log(LOG_WARNING,
		    	"%s: retrying",e.what());
		    sleep(10);
		}
	    }
	    iochan = new nidas::core::Socket(sock);
	}

	// SortedSampleStream owns the iochan ptr.
	SortedSampleInputStream input(iochan);
	input.setSorterLengthMsecs(sorterLength);

	cerr << "input, #sampleTags=" << input.getSampleTags().size() << endl;

	// Block while waiting for heapSize to become less than heapMax.
	input.setHeapBlock(true);
	input.setHeapMax(10000000);
	input.init();

	input.readHeader();
	SampleInputHeader header = input.getHeader();

	string systemName = header.getSystemName();

	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = Project::expandEnvVars(xmlFileName);

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

	char hostname[256];
	gethostname(hostname,sizeof(hostname));
	for ( ; sitr.hasNext(); ) {
	    server = sitr.next();
	    cerr << "hostname=" << hostname <<
	    	" server=" << server->getName() << endl;

	    if (server->getName().length() == 0 ||
	    	server->getName() == hostname) {

		ProcessorIterator pitr = server->getProcessorIterator();
		for ( ; pitr.hasNext(); ) {
		    SampleIOProcessor* proc = pitr.next();
		    sproc = dynamic_cast<StatisticsProcessor*>(proc);
		    if (sproc) break;
		}
		if (sproc) break;
	    }
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
	catch (n_u::EOFException& e) {
	    cerr << "EOF received: flushing buffers" << endl;
	    input.flush();
	    sproc->disconnect(&input);

	    input.close();
	    // sproc->disconnected(output);
	    throw e;
	}
	catch (n_u::IOException& e) {
	    input.close();
	    sproc->disconnect(&input);
	    // sproc->disconnected(output);
	    throw e;
	}
    }
    catch (n_u::EOFException& eof) {
        cerr << eof.what() << endl;
	return 1;
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (XMLException& e) {
        cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

