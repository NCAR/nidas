// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <ctime>

#include <unistd.h>
#include <iomanip>

#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/Datasets.h>
#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Variable.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/StatisticsProcessor.h>
#include <nidas/dynld/AsciiOutput.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Version.h>
#include <nidas/util/Logger.h>
#include <nidas/util/Process.h>

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
    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    int getLogLevel() const { return _logLevel; }

    bool getFillGaps() const { return _fillGaps; }

    int listOutputSamples();

    Dataset getDataset() throw(n_u::InvalidParameterException, XMLException);

private:

    string _argv0;

    static bool _interrupted;

    string _xmlFileName;

    list<string> _dataFileNames;

    string _dsmName;

    string _configName;

    auto_ptr<n_u::SocketAddress> _sockAddr;

    static const int DEFAULT_PORT = 30000;

    float _sorterLength;

    bool _daemonMode;

    n_u::UTime _startTime;

    n_u::UTime _endTime;

    int _niceValue;

    static const int DEFAULT_PERIOD = 300;

    int _period;

    string _configsXMLName;

    static const char* _rafXML;

    static const char* _isffXML;

    int _logLevel;

    bool _fillGaps;

    bool _doListOutputSamples;

    vector<unsigned int> _selectedOutputSampleIds;

    string _datasetName;

};

/* static */
const char* StatsProcess::_rafXML = "$PROJ_DIR/projects/$PROJECT/$AIRCRAFT/nidas/flights.xml";

/* static */
const char* StatsProcess::_isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";

int main(int argc, char** argv)
{
    return StatsProcess::main(argc,argv);
}

/* static */
bool StatsProcess::_interrupted = false;

/* static */
void StatsProcess::sigAction(int sig, siginfo_t* siginfo, void*) {
    ILOG(("received signal ") << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1));
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            StatsProcess::_interrupted = true;
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

    if (stats._doListOutputSamples) return stats.listOutputSamples();

    n_u::LogScheme ls = n_u::Logger::getInstance()->getScheme();
    ls.clearConfigs();

    if (stats._daemonMode) {
	// fork to background, send stdout/stderr to /dev/null
	if (daemon(0,0) < 0) {
	    n_u::IOException e("statsproc","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        n_u::Logger::createInstance("statsproc",LOG_PID,LOG_LOCAL5);
        ls.setShowFields("level,message");
    }

    n_u::LogConfig lc;
    lc.level = stats.getLogLevel();
    ls.addConfig(lc);

    n_u::Logger::getInstance()->setScheme(ls);


    return stats.run();
}

StatsProcess::StatsProcess():
    _argv0(),_xmlFileName(),_dataFileNames(),_dsmName(),
    _configName(),_sockAddr(0),
    _sorterLength(5.0),_daemonMode(false),
    _startTime(LONG_LONG_MIN),_endTime(LONG_LONG_MAX),
    _niceValue(0),_period(DEFAULT_PERIOD),
    _configsXMLName(),
    _logLevel(n_u::LOGGER_INFO),
    _fillGaps(false),_doListOutputSamples(false),
    _selectedOutputSampleIds(),_datasetName()
{
}

namespace {
    // utility function to parse individual integers, or
    // a range of numbers indicated by a dash. Return
    // the vector of integers.
    vector<unsigned int> parseIds(const string& sopt)
    {
        unsigned int id1,id2;
        string::size_type ic = sopt.find('-');
        vector<unsigned int> v;
        if (ic != string::npos) {
            istringstream ist(sopt.substr(0,ic));
            ist >> id1;
            if (ist.fail()) return v;
            ist.clear();
            ist.str(sopt.substr(ic+1));
            ist >> id2;
            if (ist.fail()) return v;
            for (unsigned int i = id1; i <= id2; i++) v.push_back(i);
        }
        else {
            istringstream ist(sopt);
            ist >> id1;
            if (ist.fail()) return v;
            v.push_back(id1);
        }
        return v;
    }
}

int StatsProcess::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    _argv0 = argv[0];

    while ((opt_char = getopt(argc, argv, "B:c:d:E:fhl:n:o:Op:s:S:vx:z")) != -1) {
	switch (opt_char) {
	case 'B':
	    try {
		_startTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case 'c':
	    _configName = optarg;
	    break;
	case 'd':
	    _dsmName = optarg;
	    break;
	case 'E':
	    try {
		_endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case 'f':
	    _fillGaps = true;
	    break;
	case 'h':
	    return usage(argv[0]);
	    break;
        case 'l':
	    {
	        istringstream ist(optarg);
		ist >> _logLevel;
		if (ist.fail()) {
                    cerr << "Invalid log level: " << optarg << endl;
                    return usage(argv[0]);
		}
	    }
            break;
	case 'n':
	    {
	        istringstream ist(optarg);
		ist >> _niceValue;
		if (ist.fail()) {
                    cerr << "Invalid nice value: " << optarg << endl;
                    return usage(argv[0]);
		}
	    }
	    break;
	case 'O':
	    _doListOutputSamples = true;
	    break;
	case 'o':
           {
                string soptarg(optarg);
                // parse integers or integer ranges separated by a commas
                for (;;) {
                    string::size_type ic = soptarg.find(',');
                    string sopt = soptarg.substr(0,ic);
                    if (sopt.length() > 0) {
                        vector<unsigned int> ids = parseIds(sopt);
                        if (ids.size() == 0) return usage(argv[0]);
                        _selectedOutputSampleIds.insert(_selectedOutputSampleIds.end(),
                                ids.begin(),ids.end());
                    }
                    if (ic == string::npos) break;
                    soptarg = soptarg.substr(ic+1);
                }
#ifdef DEBUG
                cerr << "_selectedOutputSampleIds=";
                for (unsigned int i = 0; i < _selectedOutputSampleIds.size(); i++)
                    cerr << _selectedOutputSampleIds[i] << ' ';
                cerr << endl;
#endif
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
		ist >> _sorterLength;
		if (ist.fail() || _sorterLength < 0.0 || _sorterLength > 1800.0) {
                    cerr << "Invalid sorter length: " << optarg << endl;
                    return usage(argv[0]);
		}
	    }
	    break;
	case 'S':
	    _datasetName = optarg;
	    break;
	case 'v':
	    cout << "Version: " << Version::getSoftwareVersion() << endl;
	    exit(0);
	case 'x':
	    _xmlFileName = optarg;
	    break;
	case 'z':
	    _daemonMode = true;
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
                _sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
            }
            catch(const n_u::UnknownHostException& e) {
                cerr << e.what() << endl;
                return usage(argv[0]);
            }
        }
	else if (url.length() > 5 && !url.compare(0,5,"unix:")) {
	    url = url.substr(5);
            _sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
        else _dataFileNames.push_back(url);
    }
    if (!_doListOutputSamples) {
        // must specify either:
        //  1. some data files to read, and optional begin and end times,
        //  2. a socket to connect to
        //  3. a time period and a $PROJECT environment variable
        //  3b a configuration name and a $PROJECT environment variable
        if (_dataFileNames.size() == 0 && !_sockAddr.get() &&
            _startTime.toUsecs() == LONG_LONG_MIN && _configName.length() == 0)
                return usage(argv[0]);

        if (_startTime.toUsecs() != LONG_LONG_MIN && _endTime.toUsecs() == LONG_LONG_MAX)
                 _endTime = _startTime + 7 * USECS_PER_DAY;
    }
    return 0;
}

/* static */
int StatsProcess::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-B time] [-E time] [-c configName] [-d dsmname] [-f] [-n nice]\n\
    [-p period] [-s sorterLength] [-S dataSet_name] [-x xml_file] [-z] [input ...]\n\
    -B \"yyyy mm dd HH:MM:SS\": begin time\n\
    -E \"yyyy mm dd HH:MM:SS\": end time\n\
    -c configName: (optional) name of configuration period to process, from configs.xml\n\
    -d dsmname: look for a <fileset> belonging to the given dsm to determine input file names\n\
    -f: Fill in time gaps with missing data. When reprocessing data you probably want to \n\
        set this option.  If for some reason you were reprocessing separate time periods in\n\
        one run, or if some of the archive files are missing, then you may not want statsproc\n\
        to output missing data values to the netcdf files for the skipped time periods,\n\
        and so then should omit -f.  If the netcdf files are being created in this run,\n\
        then -f is unnecessary.\n\
    -l logLevel: log level, default is 6=info. Other values are 7=debug, 5=notice, 4=warning, etc\n\
    -O: list sample ids of StatisticsProcessor output samples\n\
    -o [i] | [i-j] [,...]: select ids of output samples of StatisticsProcessor for processing\n\
    -p period: statistics period in seconds. If -S is used to specify a dataset, then the period\n\
       is read from $ISFF/projects/$PROJECT/ISFF/config/datasets.xml.\n\
       Otherwise it defaults to " << DEFAULT_PERIOD << "\n\
    -n nice: run at a lower priority (nice > 0)\n\
    -S dataSet_name: set environment variables specifed for the dataset,\n\
       as found in the xml file specifed by $NIDAS_DATASETS or \n\
       $ISFF/projects/$PROJECT/ISFF/config/datasets.xml\n\
    -s sorterLength: input data sorter length in fractional seconds\n\
    -x xml_file: if not specified, the xml file name is determined by either reading\n\
       the data file header or from $ISFF/projects/$PROJECT/ISFF/config/configs.xml\n\
    -z: run in daemon mode (in the background, log messages to syslog)\n\
    input: data input (optional). One of the following:\n\
        sock:host[:port]          Default port is " << DEFAULT_PORT << "\n\
        unix:sockpath             unix socket name\n\
        file[ file ...]           one or more archive file names separated by spaces\n\
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

#ifdef PROJECT_IS_SINGLETON
class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};
#endif

Dataset StatsProcess::getDataset() throw(n_u::InvalidParameterException, XMLException)
{
    string XMLName;
    const char* ndptr = getenv("NIDAS_DATASETS");

    if (ndptr) XMLName = string(ndptr);
    else {
        const char* isffDatasetsXML =
            "$ISFF/projects/$PROJECT/ISFF/config/datasets.xml";
        const char* ie = ::getenv("ISFF");
        const char* pe = ::getenv("PROJECT");
        if (ie && pe) XMLName = n_u::Process::expandEnvVars(isffDatasetsXML);
    }

    if (XMLName.length() == 0)
        throw n_u::InvalidParameterException("environment variables",
            "NIDAS_DATASETS, ISFF, PROJECT","not found");
    Datasets datasets;
    datasets.parseXML(XMLName);

    Dataset dataset = datasets.getDataset(_datasetName);
    dataset.putenv();

    _period = dataset.getResolutionSecs();

    return dataset;
}

int StatsProcess::run() throw()
{
    if (_niceValue > 0 && nice(_niceValue) < 0)  {
    	n_u::Logger::getInstance()->log(LOG_WARNING,"%s: nice(%d): %s",
		_argv0.c_str(),_niceValue,strerror(errno));
        return 1;
    }

    try {

        Project project;

        if (_datasetName.length() > 0) project.setDataset(getDataset());

        IOChannel* iochan = 0;

        if (_xmlFileName.length() > 0) {
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }
        XMLImplementation::terminate();

	if (_sockAddr.get()) {
            if (_xmlFileName.length() == 0) {
		const char* re = ::getenv("PROJ_DIR");
		const char* pe = ::getenv("PROJECT");
		const char* ae = ::getenv("AIRCRAFT");
		const char* ie = ::getenv("ISFF");
		if (re && pe && ae) _configsXMLName = n_u::Process::expandEnvVars(_rafXML);
		else if (ie && pe) _configsXMLName = n_u::Process::expandEnvVars(_isffXML);
		if (_configsXMLName.length() == 0)
		    throw n_u::InvalidParameterException("environment variables",
		    	"PROJ_DIR,AIRCRAFT,PROJECT or ISFF,PROJECT","not found");
		ProjectConfigs configs;
		configs.parseXML(_configsXMLName);
		ILOG(("parsed:") <<  _configsXMLName);
		// throws InvalidParameterException if no config for time
		const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
		cfg->initProject(project);
		// cerr << "cfg=" <<  cfg->getName() << endl;
                _xmlFileName = n_u::Process::expandEnvVars(cfg->getXMLName());
            }
	    n_u::Socket* sock = 0;
	    for (int i = 0; !sock && !_interrupted; i++) {
                try {
                    sock = new n_u::Socket(*_sockAddr.get());
                }
                catch(const n_u::IOException& e) {
                    if (i > 2)
                        n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: retrying",e.what());
                    sleep(10);
                }
	    }
            sock->setKeepAliveIdleSecs(60);
	    iochan = new nidas::core::Socket(sock);
            ILOG(("connected: ") <<  sock->getRemoteSocketAddress().toString());
        }
	else {
            nidas::core::FileSet* fset;

            // no file names listed in runstring
	    if (_dataFileNames.size() == 0) {
                // User has not specified the xml file. Get
                // the ProjectConfig from the configName or startTime
                // using the configs XML file, then parse the
                // XML of the ProjectConfig.
                if (_xmlFileName.length() == 0) {
                    const char* re = ::getenv("PROJ_DIR");
                    const char* pe = ::getenv("PROJECT");
                    const char* ae = ::getenv("AIRCRAFT");
                    const char* ie = ::getenv("ISFF");
                    if (re && pe && ae) _configsXMLName = n_u::Process::expandEnvVars(_rafXML);
                    else if (ie && pe) _configsXMLName = n_u::Process::expandEnvVars(_isffXML);
                    if (_configsXMLName.length() == 0)
                        throw n_u::InvalidParameterException("environment variables",
                            "PROJ_DIR,AIRCRAFT,PROJECT or ISFF,PROJECT","not found");

                    ProjectConfigs configs;
                    configs.parseXML(_configsXMLName);
                    ILOG(("parsed:") <<  _configsXMLName);
                    const ProjectConfig* cfg = 0;

                    if (_configName.length() > 0)
                        cfg = configs.getConfig(_configName);
                    else
                        cfg = configs.getConfig(_startTime);
                    cfg->initProject(project);
                    _xmlFileName = n_u::Process::expandEnvVars(cfg->getXMLName());

                    if (_startTime.toUsecs() == LONG_LONG_MIN) _startTime = cfg->getBeginTime();
                    if (_endTime.toUsecs() == LONG_LONG_MAX) _endTime = cfg->getEndTime();
                }

                list<nidas::core::FileSet*> fsets;
                if (_dsmName.length() > 0) {
                    fsets = project.findSampleOutputStreamFileSets(_dsmName);
                    if (fsets.empty()) {
                        PLOG(("Cannot find a FileSet for dsm %s", _dsmName.c_str()));
                        return 1;
                    }
                    if (fsets.size() > 1) {
                        PLOG(("Multple filesets found for dsm %s.", _dsmName.c_str()));
                        return 1;
                    }
                }
                else {
                    // Find SampleOutputStreamFileSets belonging to a server
                    fsets = project.findServerSampleOutputStreamFileSets();
                    if (fsets.size() > 1) {
                        PLOG(("Multple filesets found for server."));
                        return 1;
                    }
                    // probably no server defined, find for all dsms
                    if (fsets.empty())
                        fsets = project.findSampleOutputStreamFileSets();
                }
                if (fsets.empty()) {
                    PLOG(("Cannot find a FileSet in this configuration"));
                    return 1;
                }
                if (fsets.size() > 1) {
                    PLOG(("Multple filesets found for dsms. Pass a -d dsmname parameter to select one"));
                    return 1;
                }

                // must clone, since fsets.front() belongs to project
                fset = fsets.front()->clone();

                if (_startTime.toUsecs() != LONG_LONG_MIN) fset->setStartTime(_startTime);
                if (_endTime.toUsecs() != LONG_LONG_MAX) fset->setEndTime(_endTime);
	    }
	    else {
                fset = nidas::core::FileSet::getFileSet(_dataFileNames);
            }
	    iochan = fset;
	}

        RawSampleInputStream sis(iochan);
        SamplePipeline pipeline;
        pipeline.setRealTime(false);
	pipeline.setRawSorterLength(1.0);
	pipeline.setProcSorterLength(_sorterLength);
        pipeline.setRawHeapMax(1 * 1000 * 1000);
        pipeline.setProcHeapMax(1 * 1000 * 1000);

        if (_xmlFileName.length() == 0) {
            sis.readInputHeader();
            const SampleInputHeader& header = sis.getInputHeader();
	    DLOG(("header archive=") << header.getArchiveVersion() << '\n' <<
		    "software=" << header.getSoftwareVersion() << '\n' <<
		    "project=" << header.getProjectName() << '\n' <<
		    "system=" << header.getSystemName() << '\n' <<
		    "config=" << header.getConfigName() << '\n' <<
		    "configversion=" << header.getConfigVersion());

            // parse the config file.
            _xmlFileName = header.getConfigName();
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }

        project.setConfigName(_xmlFileName);

        StatisticsProcessor* sproc = 0;

        if (_dsmName.length() > 0) {
            const DSMConfig* dsm = project.findDSM(_dsmName);
            if (dsm) {
                ProcessorIterator pitr = dsm->getProcessorIterator();
                for ( ; pitr.hasNext(); ) {
                    SampleIOProcessor* proc = pitr.next();
                    StatisticsProcessor* sp = 0;
                    sp = dynamic_cast<StatisticsProcessor*>(proc);
                    if (!sp) continue;
                    // cerr << "sp period=" << sp->getPeriod() << " _period=" << _period << endl;
                    // cerr << "period diff=" << (sp->getPeriod() - _period) <<
                      //   " equality=" << (sp->getPeriod() == _period) << endl;
                    if (fabs(sp->getPeriod()-_period) < 1.e-3) {
                        sp->setFillGaps(getFillGaps());
                        sproc = sp;
                        SensorIterator si = dsm->getSensorIterator();
                        for (; si.hasNext(); ) {
                            DSMSensor* sensor = si.next();
                            sensor->init();
                            sis.addSampleTag(sensor->getRawSampleTag());
                            SampleTagIterator sti = sensor->getSampleTagIterator();
                            for ( ; sti.hasNext(); ) {
                                const SampleTag* stag = sti.next();
                                pipeline.getProcessedSampleSource()->addSampleTag(stag);
                            }
                        }
                        break;
                    }
                }
            }
        }
        if (!sproc) {
            // Find a server with a StatisticsProcessor
            list<DSMServer*> servers = project.findServers(_dsmName);
            DSMServer* server;
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
                        sp->setFillGaps(getFillGaps());
                        sproc = sp;
                        SensorIterator si = server->getSensorIterator();
                        for (; si.hasNext(); ) {
                            DSMSensor* sensor = si.next();
                            sensor->init();
                            sis.addSampleTag(sensor->getRawSampleTag());
                            SampleTagIterator sti = sensor->getSampleTagIterator();
                            for ( ; sti.hasNext(); ) {
                                const SampleTag* stag = sti.next();
                                pipeline.getProcessedSampleSource()->addSampleTag(stag);
                            }
                        }
                        break;
                    }
                }
            }
	}
	if (!sproc) {
            if (_dsmName.length() > 0)
                PLOG(("Cannot find a StatisticsProcessor for dsm or server \"%s\" with period=%d",
                    _dsmName.c_str(),_period));
            else
                PLOG(("Cannot find a StatisticsProcessor for a server with period=%d",
                    _period));
	    return 1;
	}

        if (_selectedOutputSampleIds.size() > 0) sproc->selectRequestedSampleTags(_selectedOutputSampleIds);

	try {
            if (_startTime.toUsecs() != LONG_LONG_MIN) {
                ILOG(("Searching for time ") <<
                    _startTime.format(true,"%Y %m %d %H:%M:%S"));
                sis.search(_startTime);
                ILOG(("done."));
                sproc->setStartTime(_startTime);
            }

            if (_endTime.toUsecs() != LONG_LONG_MAX)
                sproc->setEndTime(_endTime);

            pipeline.connect(&sis);
            sproc->connect(&pipeline);
            // cerr << "#sampleTags=" << sis.getSampleTags().size() << endl;

            if (_sockAddr.get()) {
                SampleOutputRequestThread::getInstance()->start();
            }
            else {
                const std::list<SampleOutput*>& outputs = sproc->getOutputs();
                std::list<SampleOutput*>::const_iterator oi = outputs.begin();
                for ( ; oi != outputs.end(); ++oi) {
                    SampleOutput* output = *oi;
                    output->requestConnection(sproc);
                }
            }

	    for (;;) {
		if (_interrupted) break;
		sis.readSamples();
	    }
	}
	catch (n_u::EOFException& e) {
	    ILOG(("EOF received"));
	}
	catch (n_u::IOException& e) {
            pipeline.disconnect(&sis);
	    sis.close();
            pipeline.flush();
            pipeline.join();
	    sproc->disconnect(&pipeline);
	    throw e;
	}
        pipeline.disconnect(&sis);
        sis.close();
        pipeline.flush();
        pipeline.interrupt();
        pipeline.join();
        sproc->disconnect(&pipeline);
    }
    catch (n_u::Exception& e) {
        // caution, don't use PLOG((e.what())), because e.what() may
        // contain format descriptors like %S from the input
        // file name format, which causes the printf inside PLOG to crash
        // looking for a matching argument. Use PLOG(("%s",e.what())) instead.
        PLOG(("%s",e.what()));
        SampleOutputRequestThread::destroyInstance();
        XMLImplementation::terminate();
	return 1;
    }
    SampleOutputRequestThread::destroyInstance();
    return 0;
}

int StatsProcess::listOutputSamples()
{
    try {

        Project project;

        if (_xmlFileName.length() > 0) {
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }
        else {
            const char* re = ::getenv("PROJ_DIR");
            const char* pe = ::getenv("PROJECT");
            const char* ae = ::getenv("AIRCRAFT");
            const char* ie = ::getenv("ISFF");
            if (re && pe && ae) _configsXMLName = n_u::Process::expandEnvVars(_rafXML);
            else if (ie && pe) _configsXMLName = n_u::Process::expandEnvVars(_isffXML);
            if (_configsXMLName.length() == 0)
                throw n_u::InvalidParameterException("environment variables",
                    "PROJ_DIR,AIRCRAFT,PROJECT or ISFF,PROJECT","not found");
            ProjectConfigs configs;
            configs.parseXML(_configsXMLName);
            ILOG(("parsed:") <<  _configsXMLName);
            // throws InvalidParameterException if no config for time
            const ProjectConfig* cfg = 0;

            if (_configName.length() > 0)
                cfg = configs.getConfig(_configName);
            else if (_startTime.toUsecs() > LONG_LONG_MIN) 
                cfg = configs.getConfig(_startTime);
            else
                cfg = configs.getConfig(n_u::UTime());

            cfg->initProject(project);
            // cerr << "cfg=" <<  cfg->getName() << endl;
        }

        XMLImplementation::terminate();

        StatisticsProcessor* sproc = 0;

        if (_dsmName.length() > 0) {
            const DSMConfig* dsm = project.findDSM(_dsmName);
            if (dsm) {
                ProcessorIterator pitr = dsm->getProcessorIterator();
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
        }
        if (!sproc) {
            // Find a server with a StatisticsProcessor
            list<DSMServer*> servers = project.findServers(_dsmName);
            DSMServer* server;
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
	}
	if (!sproc) {
            if (_dsmName.length() > 0)
                PLOG(("Cannot find a StatisticsProcessor for dsm or server \"%s\" with period=%d",
                    _dsmName.c_str(),_period));
            else
                PLOG(("Cannot find a StatisticsProcessor for a server with period=%d",
                    _period));
	    return 1;
	}
        std::list<const SampleTag*> tags =  sproc->getRequestedSampleTags();
        std::list<const SampleTag*>::const_iterator ti = tags.begin();
        for ( ; ti != tags.end(); ++ti) {
            const SampleTag* stag = *ti;

            cout << setw(4) << (stag->getSpSId() - sproc->getSampleId()) << ' ';
            const std::vector<const Variable*>& vars = stag->getVariables();
            std::vector<const Variable*>::const_iterator vi = vars.begin();
            for ( ; vi != vars.end(); ++vi) {
                const Variable* var = *vi;
                cout << var->getName() << ' ';
            }
            cout << endl;
        }
    }
    catch (n_u::Exception& e) {
        // caution, don't use PLOG((e.what())), because e.what() may
        // contain format descriptors like %S from the input
        // file name format, which causes the printf inside PLOG to crash
        // looking for a matching argument. Use PLOG(("%s",e.what())) instead.
        PLOG(("%s",e.what()));
        XMLImplementation::terminate();
	return 1;
    }
    return 0;
}
