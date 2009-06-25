/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <ctime>

#include <unistd.h>

#include <nidas/core/Project.h>
#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/StatisticsProcessor.h>
#include <nidas/dynld/AsciiOutput.h>
#include <nidas/core/XMLParser.h>
#include <nidas/util/Logger.h>
#include <nidas/core/Version.h>

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

    string argv0;

    static bool interrupted;

    string xmlFileName;

    list<string> dataFileNames;

    string dsmName;

    string configName;

    auto_ptr<n_u::SocketAddress> sockAddr;

    static const int DEFAULT_PORT = 30000;

    int sorterLength;

    bool daemonMode;

    n_u::UTime startTime;

    n_u::UTime endTime;

    int niceValue;

    static const int DEFAULT_PERIOD = 300;

    int _period;

    string configsXMLName;

    static const char* rafXML;

    static const char* isffXML;


};

/* static */
const char* StatsProcess::rafXML = "$PROJ_DIR/projects/$PROJECT/$AIRCRAFT/nidas/flights.xml";

/* static */
const char* StatsProcess::isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";


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
int StatsProcess::main(int argc, char** argv) throw()
{
    setupSignals();

    StatsProcess stats;

    int res;
    
    if ((res = stats.parseRunstring(argc,argv)) != 0) return res;

    if (stats.daemonMode) {
	// fork to background, send stdout/stderr to /dev/null
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMServer","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        n_u::Logger::createInstance("statsproc",LOG_CONS,LOG_LOCAL5);
    }

    n_u::LogConfig lc;
    lc.level = n_u::LOGGER_INFO;
    n_u::Logger::getInstance()->setScheme(
        n_u::LogScheme().addConfig (lc));


    return stats.run();
}


StatsProcess::StatsProcess():
	sorterLength(1000),daemonMode(false),
        startTime((time_t)0),endTime((time_t)0),
        niceValue(0),_period(DEFAULT_PERIOD)
{
}

int StatsProcess::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    argv0 = argv[0];

    while ((opt_char = getopt(argc, argv, "B:c:d:E:hn:p:s:vx:z")) != -1) {
	switch (opt_char) {
	case 'B':
	    try {
		startTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case 'c':
	    configName = optarg;
	    break;
	case 'd':
	    dsmName = optarg;
	    break;
	case 'E':
	    try {
		endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case 'h':
	    return usage(argv[0]);
	    break;
	case 'n':
	    {
	        istringstream ist(optarg);
		ist >> niceValue;
		if (ist.fail()) {
                    cerr << "Invalid nice value: " << optarg << endl;
                    return usage(argv[0]);
		}
	    }
	    break;
	case 'p':
	    {
	        istringstream ist(optarg);
		ist >> _period;
		if (ist.fail() || _period < 0) {
                    cerr << "Invalid period: " << optarg << endl;
                    return usage(argv[0]);
		}
	    }
	    break;
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
	case 'v':
	    cout << "Version: " << Version::getSoftwareVersion() << endl;
	    exit(0);
	case 'x':
	    xmlFileName = optarg;
	    break;
	case 'z':
	    daemonMode = true;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    for ( ; optind < argc; optind++) {
        string url(argv[optind]);
        if (url.length() > 5 && !url.compare(0,5,"sock:")) {
            url = url.substr(5);
	    string hostName = "127.0.0.1";
            int port = DEFAULT_PORT;
	    if (url.length() > 0) {
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
            try {
                n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
                sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
            }
            catch(const n_u::UnknownHostException& e) {
                cerr << e.what() << endl;
                return usage(argv[0]);
            }
        }
	else if (url.length() > 5 && !url.compare(0,5,"unix:")) {
	    url = url.substr(5);
            sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
        else dataFileNames.push_back(url);
    }
    // must specify either:
    //  1. some data files to read, and optional begin and end times,
    //  2. a socket to connect to
    //  3. a time period and a $PROJECT environment variable
    //  3b a configuration name and a $PROJECT environment variable
    if (dataFileNames.size() == 0 && !sockAddr.get() &&
        startTime.toUsecs() == 0 && configName.length() == 0)
            return usage(argv[0]);

    if (startTime.toUsecs() != 0 && endTime.toUsecs() == 0)
             endTime = startTime + 7 * USECS_PER_DAY;
    return 0;
}

/* static */
int StatsProcess::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-B time] [-E time] [-c configName] [-d dsm] [-n nice] [-p period] [-s sorterLength]\n\
       [-x xml_file] [-z] [input ...]\n\
    -B \"yyyy mm dd HH:MM:SS\": begin time\n\
    -E \"yyyy mm dd HH:MM:SS\": end time\n\
    -c configName: (optional) name of configuration period to process, from configs.xml\n\
    -d dsm: (optional)\n\
    -p period: statistics period in seconds, default = " << DEFAULT_PERIOD << "\n\
    -n nice: run at a lower priority (nice > 0)\n\
    -s sorterLength: input data sorter length in milliseconds\n\
    -x xml_file: if not specified, the xml file name is determined by either reading\n\
       the data file header or from $ISFF/projects/$PROJECT/ISFF/config/configs.xml\n\
    -z: run in daemon mode (in the background, log messages to syslog)\n\
    input: data input (optional). One of the following:\n\
        sock:host[:port]          Default port is " << DEFAULT_PORT << "\n\
        unix:sockpath             unix socket name\n\
        file[,file,...]           one or more archive file names\n\
\n\
If no inputs are specified, then the -B time option must be given, and\n" <<
argv0 << " will read $ISFF/projects/$PROJECT/ISFF/config/configs.xml, to\n\
find an xml configuration for the begin time, read it to find a\n\
<fileset> archive for the dsm, and then open data files\n\
matching the <fileset> path descriptor and time period.\n\
\n" <<
argv0 << " scans the xml file for a <processor> of class StatisticsProcessor\n\
in order to determine what statistics to generate.\n\
\n\
Examples:\n" <<
	argv0 << " -B \"2006 jun 10 00:00\" -E \"2006 jul 3 00:00\"\n" <<
	argv0 << " sock:dsmhost\n" <<
	argv0 << " unix:/tmp/data_socket\n" <<
        endl;
    return 1;
}

int StatsProcess::run() throw()
{
    if (niceValue > 0 && nice(niceValue) < 0)  {
    	n_u::Logger::getInstance()->log(LOG_WARNING,"%s: nice(%d): %s",
		argv0.c_str(),niceValue,strerror(errno));
        return 1;
    }

    if (dsmName.length() == 0) {
        char hostname[256];
        gethostname(hostname,sizeof(hostname));
        dsmName = hostname;
    }

    auto_ptr<Project> project;
    auto_ptr<SortedSampleInputStream> sis;
    IOChannel* iochan = 0;
    nidas::core::FileSet* fset = 0;
    try {
        if (xmlFileName.length() > 0) {
            xmlFileName = Project::expandEnvVars(xmlFileName);
            XMLParser parser;
            cerr << "parsing: " << xmlFileName << endl;
            auto_ptr<xercesc::DOMDocument> doc(parser.parse(xmlFileName));
            project.reset(Project::getInstance());
            project->fromDOMElement(doc->getDocumentElement());
        }

	if (sockAddr.get()) {
            if (!project.get()) {
		const char* re = getenv("PROJ_DIR");
		const char* pe = getenv("PROJECT");
		const char* ae = getenv("AIRCRAFT");
		const char* ie = getenv("ISFF");
		if (re && pe && ae) configsXMLName = Project::expandEnvVars(rafXML);
		else if (ie && pe) configsXMLName = Project::expandEnvVars(isffXML);
		if (configsXMLName.length() == 0)
		    throw n_u::InvalidParameterException("environment variables",
		    	"PROJ_DIR,AIRCRAFT,PROJECT or ISFF,PROJECT","not found");
		ProjectConfigs configs;
		configs.parseXML(configsXMLName);
		cerr << "parsed:" <<  configsXMLName << endl;
		// throws InvalidParameterException if no config for time
		const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
		project.reset(cfg->getProject());
		cerr << "cfg=" <<  cfg->getName() << endl;
		xmlFileName = cfg->getXMLName();
            }
	    n_u::Socket* sock = 0;
	    for (int i = 0; !sock && !interrupted; i++) {
                try {
                    sock = new n_u::Socket(*sockAddr.get());
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
	else {
	    if (dataFileNames.size() == 0) {
                // User has not specified the xml file. Get
                // the ProjectConfig from the configName or startTime
                // using the configs XML file, then parse the
                // XML of the ProjectConfig.
                if (!project.get()) {
                    string configsXML = Project::expandEnvVars(
                        "$ISFF/projects/$PROJECT/ISFF/config/configs.xml");

                    ProjectConfigs configs;
                    configs.parseXML(configsXML);
                    const ProjectConfig* cfg = 0;

                    if (configName.length() > 0)
                        cfg = configs.getConfig(configName);
                    else
                        cfg = configs.getConfig(startTime);
                    project.reset(cfg->getProject());
                    if (startTime.toUsecs() == 0) startTime = cfg->getBeginTime();
                    if (endTime.toUsecs() == 0) endTime = cfg->getEndTime();
                }
	        list<nidas::core::FileSet*> fsets = project->findSampleOutputStreamFileSets(
			dsmName);
		if (fsets.size() == 0) {
		    n_u::Logger::getInstance()->log(LOG_ERR,
		    "Cannot find a FileSet for dsm %s",
		    	dsmName.c_str());
		    return 1;
		}
                // must clone, since fsets.front() belongs to project
                fset = fsets.front()->clone();

                if (startTime.toUsecs() != 0) fset->setStartTime(startTime);
                if (endTime.toUsecs() != 0) fset->setEndTime(endTime);
	    }
	    else {
                fset = new nidas::core::FileSet();
                list<string>::const_iterator fi;
                for (fi = dataFileNames.begin();
                    fi != dataFileNames.end(); ++fi)
                        fset->addFileName(*fi);
            }
	    iochan = fset;
	}

        sis.reset(new SortedSampleInputStream(iochan));
        sis->setHeapBlock(true);
        sis->setHeapMax(10000000);
        sis->init();
	sis->setSorterLengthMsecs(sorterLength);

        if (!project.get()) {
            sis->readInputHeader();
            const SampleInputHeader& header = sis->getInputHeader();
	    cerr << "header archive=" << header.getArchiveVersion() << '\n' <<
		    "software=" << header.getSoftwareVersion() << '\n' <<
		    "project=" << header.getProjectName() << '\n' <<
		    "system=" << header.getSystemName() << '\n' <<
		    "config=" << header.getConfigName() << '\n' <<
		    "configversion=" << header.getConfigVersion() << endl;

            // parse the config file.
            xmlFileName = header.getConfigName();
            xmlFileName = Project::expandEnvVars(xmlFileName);
            XMLParser parser;
            auto_ptr<xercesc::DOMDocument> doc(parser.parse(xmlFileName));
            project.reset(Project::getInstance());
            project->fromDOMElement(doc->getDocumentElement());
        }

	// Find a server with a StatisticsProcessor
	list<DSMServer*> servers = project->findServers(dsmName);
	if (servers.size() == 0) {
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    "Cannot find a DSMServer for dsm %s",
		dsmName.c_str());
	    return 1;
	}
        DSMServer* server = 0;
	StatisticsProcessor* sproc = 0;
	list<DSMServer*>::const_iterator svri = servers.begin();
        for ( ; !sproc && svri != servers.end(); ++svri) {
            server = *svri;
            ProcessorIterator pitr = server->getProcessorIterator();
            for ( ; pitr.hasNext(); ) {
                SampleIOProcessor* proc = pitr.next();
                StatisticsProcessor* sp = 0;
                sp = dynamic_cast<StatisticsProcessor*>(proc);
                if (!sp) continue;
                // cerr << "sp period=" << sp->getPeriod() << " _period=" << _period << endl;
                // cerr << "period diff=" << (sp->getPeriod() - _period) <<
                  //   " equality=" << (sp->getPeriod() == _period) << endl;
                if (fabs(sp->getPeriod()-_period) < 1.e-3) {
                    sproc = sp;
                    break;
                }
            }
        }
	if (!sproc) {
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    "Cannot find a StatisticsProcessor for dsm %s with period=%d",
		dsmName.c_str(),_period);
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
	    sis->addSampleTag(stag);
	}

	sproc->connect(sis.get());
	// cerr << "#sampleTags=" << sis->getSampleTags().size() << endl;

        if (startTime.toUsecs() != 0) {
            cerr << "Searching for time " <<
                startTime.format(true,"%Y %m %d %H:%M:%S");
            sis->search(startTime);
            cerr << " done." << endl;
            sproc->setStartTime(startTime);
        }
        if (endTime.toUsecs() != 0)
            sproc->setEndTime(endTime);

	try {
	    for (;;) {
		if (interrupted) break;
		sis->readSamples();
	    }
	}
	catch (n_u::EOFException& e) {
	    cerr << "EOF received: flushing buffers" << endl;
	    sis->flush();
	    cerr << "sproc->disconnect" << endl;
	    sproc->disconnect(sis.get());
	    cerr << "sis->close" << endl;
	    sis->close();
	    // sproc->disconnected(output);
	    throw e;
	}
	catch (n_u::IOException& e) {
	    sis->close();
	    sproc->disconnect(sis.get());
	    // sproc->disconnected(output);
	    throw e;
	}
    }
    catch (n_u::EOFException& eof) {
        cerr << eof.what() << endl;
	return 0;
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

