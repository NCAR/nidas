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

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/NearestResampler.h>
#include <nidas/core/NearestResamplerAtRate.h>
#include <nidas/core/XMLParser.h>

#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/Version.h>
#include <nidas/core/Socket.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Process.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <memory> // auto_ptr<>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { ASCII, BINARY1, BINARY2 } format_t;

    DumpClient(format_t,ostream&, int asciiPrecision);

    virtual ~DumpClient() {}

    bool receive(const Sample* samp) throw();

    void printHeader(vector<const Variable*> vars);

    void setStartTime(const n_u::UTime& val)
    {
        startTime = val;
        checkStart = true;
    }

    void setEndTime(const n_u::UTime& val)
    {
        endTime = val;
        checkEnd = true;
    }

    void setDOS(bool val)
    {
        dosOut = val;
    }

    bool getDOS() const
    {
        return dosOut;
    }

private:

    format_t format;

    ostream& ostr;

    n_u::UTime startTime;

    n_u::UTime endTime;

    bool checkStart;

    bool checkEnd;

    bool dosOut;

    int asciiPrecision;

};


class DataPrep
{
public:

    DataPrep();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    static int main(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    vector<const Variable*> matchVariables(Project* project,
        set<const DSMConfig*>& activeDsms,
        set<DSMSensor*>& activeSensors) throw (n_u::InvalidParameterException);

    static void interrupt() { interrupted = true; }

    static void finishUp() { finished = true; }

private:

    string progname;

    Project* project;

    IOChannel* iochan;

    static bool interrupted;

    static bool finished;

    string xmlFileName;

    list<string> dataFileNames;

    auto_ptr<n_u::SocketAddress> sockAddr;

    static const int DEFAULT_PORT = 30000;

    float sorterLength;

    DumpClient::format_t format;

    list<Variable*> reqVars;

    n_u::UTime startTime;

    n_u::UTime endTime;

    std::string configName;

    float rate;

    bool dosOut;

    bool doHeader;

    static const char* rafXML;

    static const char* isffXML;

    int asciiPrecision;

};

/* static */
const char* DataPrep::rafXML = "$PROJ_DIR/projects/$PROJECT/$AIRCRAFT/nidas/flights.xml";

/* static */
const char* DataPrep::isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";

DumpClient::DumpClient(format_t fmt,ostream &outstr,int precision):
	format(fmt),ostr(outstr),startTime((time_t)0),endTime((time_t)0),
        checkStart(false),checkEnd(false),dosOut(false),
        asciiPrecision(precision)
{
}

void DumpClient::printHeader(vector<const Variable*>vars)
{
    // cout << "|--- date time -------| deltaT   bytes" << endl;
    vector<const Variable*>::const_iterator vi = vars.begin();
    for (; vi != vars.end(); ++vi) {
        const Variable* var = *vi;
        cout << var->getName() << ' ';
    }
    if (dosOut) cout << '\r';
    cout << endl;
    vi = vars.begin();
    for (; vi != vars.end(); ++vi) {
        const Variable* var = *vi;
        if (var->getConverter())
            cout << '"' << var->getConverter()->getUnits() << "\" ";
        else
            cout << '"' << var->getUnits() << "\" ";
    }
    if (dosOut) cout << '\r';
    cout << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    if (checkStart && tt < startTime.toUsecs()) return false;
    if (checkEnd && tt > endTime.toUsecs()) {
        DataPrep::finishUp();
        return false;
    }

#ifdef DEBUG
    cerr << "sampid=" << GET_DSM_ID(samp->getId()) << ',' <<
    	GET_SHORT_ID(samp->getId()) << endl;
#endif

    switch(format) {
    case ASCII:
	{
	n_u::UTime ut(tt);
	ostr << ut.format(true,"%Y %m %d %H:%M:%S.%4f");

	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	ostr << setprecision(asciiPrecision) << setfill(' ');
        // last value is number of non-NAs
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float) - 1; i++)
	    ostr << ' ' << setw(10) << fp[i];
        if (dosOut) cout << '\r';
	ostr << endl;
	}
        break;
    case BINARY1:
	{

	int fsecs = tt % USECS_PER_SEC;
	double ut = (double)((tt - fsecs) / USECS_PER_SEC) +
            (double) fsecs / USECS_PER_SEC;

	ostr.write((const char*)&ut,sizeof(ut));
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float) - 1; i++)
	    ostr.write((const char*)(fp+i),sizeof(float));
	}
        break;
    case BINARY2:
	{

	ostr.write((const char*)&tt,sizeof(tt));
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float); i++)
	    ostr.write((const char*)(fp+i),sizeof(float));
	}
        break;
    }
    return true;
}

DataPrep::DataPrep(): 
        sorterLength(1.00),
	format(DumpClient::ASCII),
        startTime((time_t)0),endTime((time_t)0),
        rate(0.0),dosOut(false),doHeader(true),
        asciiPrecision(5)
{
}

int DataPrep::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
    char* p1,*p2;

    progname = argv[0];

    while ((opt_char = getopt(argc, argv, "AB:CD:dE:hHp:r:s:vx:")) != -1) {
	switch (opt_char) {
	case 'A':
	    format = DumpClient::ASCII;
	    break;
	case 'B':
	    try {
		startTime = n_u::UTime::parse(true,optarg);
	    }
	    catch(const n_u::ParseException& e) {
	        cerr << e.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case 'C':
	    format = DumpClient::BINARY1;
	    break;
	case 'D':
	    {
		p1 = optarg;
		while ( (p2=strchr(p1,','))) {
		    Variable *var = new Variable();
                    char* ph = strchr(p1,'#');
                    if (ph) {
                        var->setName(string(p1,ph-p1));
                        ph++;
                        istringstream strm(string(ph,p2-ph));
                        int istn;
                        strm >> istn;
                        if (strm.fail()) {
                            cerr << "cannot parse station number for variable " <<
                                string(p1,p2-p1) << endl;
                            return usage(argv[0]);
                        }
                        var->setStation(istn);
                    } else var->setName(string(p1,p2-p1));
		    reqVars.push_back(var);
		    p1 = p2 + 1;
		}

		Variable *var = new Variable();
                char* ph = strchr(p1,'#');
                if (ph) {
                    var->setName(string(p1,ph-p1));
                    ph++;
                    istringstream strm(ph);
                    int istn;
                    strm >> istn;
                    if (strm.fail()) {
                        cerr << "cannot parse station number for variable " <<
                            string(p1) << endl;
                        return usage(argv[0]);
                    }
                    var->setStation(istn);
                } else var->setName((p1));
		reqVars.push_back(var);

	    }
	    break;
	case 'd':
	    dosOut = true;
	    break;
	case 'E':
	    try {
		endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch(const n_u::ParseException& e) {
	        cerr << e.what() << endl;
		return usage(argv[0]);
	    }
	    break;
      case 'h':
            return usage(argv[0]);
            break;
      case 'H':
            doHeader = false;
            break;
      case 'p':
            {
                istringstream ist(optarg);
                ist >> asciiPrecision;
                if (ist.fail() || asciiPrecision < 1) {
                    cerr << "Invalid precision: " << optarg << endl;
                    return usage(argv[0]);
                }
            }
            break;
      case 'r':
            {
                istringstream ist(optarg);
                ist >> rate;
                if (ist.fail() || rate < 0) {
                    cerr << "Invalid resample rate: " << optarg << endl;
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
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }

    if (reqVars.size() == 0) {
        cerr << "no variables requested, must have one or more -D options" <<
            endl;
        return usage(argv[0]);
    }

    for ( ; optind < argc; optind++) {
	string url(argv[optind]);
	if (url.length() > 5 && url.substr(0,5) == "sock:") {
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
	else if (url.length() > 5 && url.substr(0,5) == "unix:") {
	    url = url.substr(5);
            sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
	else dataFileNames.push_back(url);
    }
    // must specify either:
    //	1. some data files to read, and optional begin and end times,
    //  2. a socket to connect to
    //	3. or a time period and a $PROJECT environment variable
    if (dataFileNames.size() == 0 && !sockAddr.get() &&
    	startTime.toUsecs() == 0) return usage(argv[0]);
    if (startTime.toUsecs() != 0 && endTime.toUsecs() == 0)
        endTime = startTime + 90 * USECS_PER_DAY;
    return 0;
}

int DataPrep::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-A] [-C] -D var[,var,...] [-B time] [-E time]\n\
        [-h] [-r rate] [-s sorterLength] [-x xml_file] [input ...]\n\
    -A :ascii output (default)\n\
    -C :binary column output, double seconds since Jan 1, 1970, followed by floats for each var\n\
    -d : dos output (records terminated by CRNL instead of just NL)\n\
    -D var[,var,...]: One or more variable names to display\n\
    -B \"yyyy mm dd HH:MM:SS\": begin time (optional)\n\
    -E \"yyyy mm dd HH:MM:SS\": end time (optional)\n\
    -h : this help\n\
    -H : don't print out initial two line ASCII header of variable names and units\n\
    -p precision: number of digits in ASCII output values, default is 5\n\
    -r rate: optional resample rate, in Hz (optional)\n\
    -s sorterLength: input data sorter length in seconds (optional)\n\
    -v : show version\n\
    -x xml_file: if not specified, the xml file name is determined by either reading\n\
       the data file header or from $ISFF/projects/$PROJECT/ISFF/config/configs.xml\n\
    input: data input (optional). One of the following:\n\
        sock:host[:port]          Default port is " << DEFAULT_PORT << "\n\
        unix:sockpath             unix socket name\n\
        file[,file,...]           one or more archive file names\n\
\n\
If no inputs are specified, then the -B time option must be given, and\n" <<
argv0 << " will read $ISFF/projects/$PROJECT/ISFF/config/configs.xml, to\n\
find an xml configuration for the begin time, read it to find a\n\
<fileset> archive for the given variables, and then open data files\n\
matching the <fileset> path descriptor and time period.\n\
\n" <<
argv0 << " does simple resampling, using the nearest sample to the times of the first\n\
variable requested, or if the -r rate option is specified, to evenly spaced times\n\
at the given rate.\n\
\n\
Examples:\n" <<
	argv0 << " -D u.10m,v.10m,w.10m -B \"2006 jun 10 00:00\" -E \"2006 jul 3 00:00\"\n" <<
	argv0 << " -D u.10m,v.10m,w.10m sock:dsmhost\n" <<
	argv0 << " -D u.10m,v.10m,w.10m -r 60 unix:/tmp/data_socket\n" <<
        endl;
    return 1;
}

/* static */
bool DataPrep::interrupted = false;

bool DataPrep::finished = false;

/* static */
void DataPrep::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            DataPrep::interrupted = true;
    break;
    }
}

/* static */
void DataPrep::setupSignals()
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
    act.sa_sigaction = DataPrep::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int DataPrep::main(int argc, char** argv)
{
    setupSignals();

    n_u::LogConfig lc;
    lc.level = n_u::LOGGER_INFO;
    n_u::Logger::getInstance()->setScheme(
        n_u::LogScheme().addConfig (lc));

    DataPrep dump;

    int res;

    if ((res = dump.parseRunstring(argc,argv))) return res;

    return dump.run();
}

vector<const Variable*> DataPrep::matchVariables(Project* project,
    set<const DSMConfig*>& activeDsms,
    set<DSMSensor*>& activeSensors) throw (n_u::InvalidParameterException)
{
    vector<const Variable*> variables;
    list<Variable*>::const_iterator rvi = reqVars.begin();
    for ( ; rvi != reqVars.end(); ++rvi) {
        Variable* reqvar = *rvi;
        bool match = false;

        DSMConfigIterator di = project->getDSMConfigIterator();
        for ( ; !match && di.hasNext(); ) {
            const DSMConfig* dsm = di.next();

            const list<DSMSensor*>& sensors = dsm->getSensors();
            list<DSMSensor*>::const_iterator si = sensors.begin();
            for (; !match && si != sensors.end(); ++si) {
                DSMSensor* sensor = *si;
                VariableIterator vi = sensor->getVariableIterator();
                for ( ; !match && vi.hasNext(); ) {
                    const Variable* var = vi.next();
#ifdef DEBUG
                    cerr << "var=" << var->getName() <<
                        "(" << var->getStation() << "), " <<
                        var->getNameWithoutSite() <<
                        ", reqvar=" << reqvar->getName() <<
                        "(" << reqvar->getStation() << "), " <<
                        var->getNameWithoutSite() << endl;
#endif
                    if (*var == *reqvar) {
                        variables.push_back(var);
                        activeSensors.insert(sensor);
                        activeDsms.insert(dsm);
                        match = true;
                        break;
                    }
                }
            }
        }
        if (!match) throw
            n_u::InvalidParameterException(
                progname,"cannot find variable",reqvar->getName());
    }
    return variables;
}

int DataPrep::run() throw()
{
    try {

        auto_ptr<Project> project;

        IOChannel* iochan = 0;

        if (xmlFileName.length() > 0) {

            xmlFileName = n_u::Process::expandEnvVars(xmlFileName);
            XMLParser parser;
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
                string configsXMLName;
                if (re && pe && ae) configsXMLName = n_u::Process::expandEnvVars(rafXML);
                else if (ie && pe) configsXMLName = n_u::Process::expandEnvVars(isffXML);
                if (configsXMLName.length() == 0)
                    throw n_u::InvalidParameterException("environment variables",
                        "PROJ_DIR,AIRCRAFT,PROJECT or ISFF,PROJECT","not found");
                ProjectConfigs configs;
                configs.parseXML(configsXMLName);
                cerr << "parsed:" <<  configsXMLName << endl;
                // throws InvalidParameterException if no config for time
                const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
                project.reset(cfg->getProject());
                // cerr << "cfg=" <<  cfg->getName() << endl;
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
            IOChannel* iosock = new nidas::core::Socket(sock);
            iochan = iosock->connect();
            if (iochan != iosock) {
                iosock->close();
                delete iosock;
            }
        }
        else {
            nidas::core::FileSet* fset;
            if (dataFileNames.size() == 0) {
                // User has not specified the xml file. Get
                // the ProjectConfig from the configName or startTime
                // using the configs XML file, then parse the
                // XML of the ProjectConfig.
                if (!project.get()) {
                    string configsXML = n_u::Process::expandEnvVars(
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
                list<nidas::core::FileSet*> fsets = project->findSampleOutputStreamFileSets();
                if (fsets.size() == 0) {
                    n_u::Logger::getInstance()->log(LOG_ERR,"Cannot find a FileSet");
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

	RawSampleInputStream sis(iochan);
        SamplePipeline pipeline;
        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(1.0);
        pipeline.setProcSorterLength(sorterLength);
        pipeline.setRawHeapMax(100 * 1000 * 1000);
        pipeline.setProcHeapMax(500 * 1000 * 1000);

        if (!project.get()) {

            sis.readInputHeader();
            const SampleInputHeader& header = sis.getInputHeader();
	    xmlFileName = header.getConfigName();
            xmlFileName = n_u::Process::expandEnvVars(xmlFileName);
            XMLParser parser;
	    auto_ptr<xercesc::DOMDocument> doc(parser.parse(xmlFileName));

	    project.reset(Project::getInstance());
	    project->fromDOMElement(doc->getDocumentElement());

	    list<Variable*>::const_iterator rvi = reqVars.begin();
        }

        // match the variables.
        // on a match:
        //	1. save the var to pass to the resampler
        //  2. save the sensor in a set
        //  3. save the dsm in a set
        // later:
        //	1. init the sensors
        //	2. add the resampler as a processedSampleClient of the sensor

	vector<const Variable*> variables;
	set<DSMSensor*> activeSensors;
        set<const DSMConfig*> activeDsms;
        variables = matchVariables(project.get(),activeDsms,activeSensors);

        for (unsigned int i = 0; i < variables.size(); i++)
            cerr << "var=" << variables[i]->getName() << endl;

        auto_ptr<Resampler> resampler;

        if (rate > 0.0) {
            NearestResamplerAtRate* smplr =
                new NearestResamplerAtRate(variables);
            smplr->setRate(rate);
            smplr->setFillGaps(true);
            resampler.reset(smplr);
        }
        else {
            resampler.reset(new NearestResampler(variables));
        }

	set<DSMSensor*>::const_iterator si = activeSensors.begin();
	for ( ; si != activeSensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->init();
            sis.addSampleTag(sensor->getRawSampleTag());
	}

        pipeline.connect(&sis);
        resampler->connect(pipeline.getProcessedSampleSource());

        DumpClient dumper(format,cout,asciiPrecision);
        dumper.setDOS(dosOut);

	resampler->addSampleClient(&dumper);

        if (startTime.toUsecs() != 0) {
            cerr << "searching for time " <<
                startTime.format(true,"%Y %m %d %H:%M:%S") << endl;
            sis.search(startTime);
            cerr << "search done." << endl;
            dumper.setStartTime(startTime);
        }
        if (endTime.toUsecs() != 0) dumper.setEndTime(endTime);

	if (doHeader) dumper.printHeader(variables);

        try {
            for (;;) {
                sis.readSamples();
                if (finished || interrupted) break;
            }
        }
        catch (n_u::EOFException& e) {
            cerr << "EOF received: flushing buffers" << endl;
            sis.flush();
        }
        catch (n_u::IOException& e) {
            resampler->removeSampleClient(&dumper);
            resampler->disconnect(pipeline.getProcessedSampleSource());
            pipeline.disconnect(&sis);
            sis.close();
            throw e;
        }
        resampler->removeSampleClient(&dumper);
        resampler->disconnect(pipeline.getProcessedSampleSource());
        pipeline.disconnect(&sis);
        sis.close();
    }
    catch (nidas::core::XMLException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
	cerr << e.what() << endl;
	return 1;
    }
    if (interrupted) return 1;       // interrupted
    return 0;
}

int main(int argc, char** argv)
{
    return DataPrep::main(argc,argv);
}
