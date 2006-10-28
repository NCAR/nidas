/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-04 09:51:31 -0600 (Tue, 04 Jul 2006) $

    $LastChangedRevision: 3424 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/apps/data_dump.cc $
 ********************************************************************

*/

#include <ctime>

#include <nidas/dynld/FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/NearestResampler.h>
#include <nidas/core/NearestResamplerAtRate.h>

#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/Version.h>

#include <nidas/util/UTime.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { ASCII, BINARY1, BINARY2 } format_t;

    DumpClient(format_t,ostream&);

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


private:

    format_t format;

    ostream& ostr;

    n_u::UTime startTime;

    n_u::UTime endTime;

    bool checkStart;

    bool checkEnd;

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

private:

    string progname;

    Project* project;

    IOChannel* iochan;

    static bool interrupted;

    string xmlFileName;

    list<string> dataFileNames;

    string sockHostName;

    static int defaultPort;

    int port;

    int sorterLength;

    DumpClient::format_t format;

    list<Variable*> reqVars;

    n_u::UTime startTime;

    n_u::UTime endTime;

    float rate;

};

DumpClient::DumpClient(format_t fmt,ostream &outstr):
	format(fmt),ostr(outstr),startTime((time_t)0),endTime((time_t)0),
        checkStart(false),checkEnd(false)
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
    cout << endl;
    vi = vars.begin();
    for (; vi != vars.end(); ++vi) {
        const Variable* var = *vi;
        cout << '"' << var->getUnits() << "\" ";
    }
    cout << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    if (checkStart && tt < startTime.toUsecs()) return false;
    if (checkEnd && tt > endTime.toUsecs()) {
        DataPrep::interrupt();
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
	ostr << ut.format(true,"%Y %m %d %H:%M:%S.%3f");

	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	ostr << setprecision(4) << setfill(' ');
        // last value is number of non-NAs
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float) - 1; i++)
	    ostr << setw(10) << fp[i] << ' ';
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

/* static */
int DataPrep::defaultPort = 30000;

DataPrep::DataPrep(): 
	port(defaultPort),
	sorterLength(250),
	format(DumpClient::ASCII),
        startTime((time_t)0),endTime((time_t)0),
        rate(0.0)
{
}

int DataPrep::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
    char* p1,*p2;

    progname = argv[0];

    while ((opt_char = getopt(argc, argv, "AB:CD:E:r:vx:")) != -1) {
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
	case 'E':
	    try {
		endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch(const n_u::ParseException& e) {
	        cerr << e.what() << endl;
		return usage(argv[0]);
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
	if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	    url = url.substr(5);
	    sockHostName = "127.0.0.1";
	    if (url.length() > 0) {
		size_t ic = url.find(':');
		sockHostName = url.substr(0,ic);
		if (ic < string::npos) {
		    istringstream ist(url.substr(ic+1));
		    ist >> port;
		    if (ist.fail()) {
			cerr << "Invalid port number: " << url.substr(ic+1) << endl;
			return usage(argv[0]);
		    }
		}
	    }
	}
	else if (url.length() > 5 && !url.compare(0,5,"file:")) {
	    url = url.substr(5);
	    dataFileNames.push_back(url);
	}
	else dataFileNames.push_back(url);
    }
    // must specify either:
    //	1. some data files to read, and optional begin and end times,
    //  2. a socket to connect to
    //	3. or a time period and a $PROJECT environment variable
    if (dataFileNames.size() == 0 && sockHostName.length() == 0 &&
    	startTime.toUsecs() == 0) return usage(argv[0]);
    if (startTime.toUsecs() != 0 && endTime.toUsecs() == 0)
        endTime = startTime + 31 * USECS_PER_DAY;
    return 0;
}

int DataPrep::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-B time] [-E time] [-d dsm] [-n nice] [-r rate] [-s sorterLength] [-x xml_file] [-z] [input] ...\n\
    -B \"yyyy mm dd HH:MM:SS\": begin time\n\
    -E \"yyyy mm dd HH:MM:SS\": end time\n\
    -d dsm\n\
    -n nice: run at a lower priority (nice > 0)\n\
    -r rate: resample rate, in Hz\n\
    -s sorterLength: input data sorter length in milliseconds\n\
    -x xml_file: (optional), the default value is read from the input\n\
    input: names of one or more raw data files, or sock:[hostname[:port]]\n\
\n\
sock:[hostname[:port]:  default hostname is \"localhost\", default port is " <<
	defaultPort << "\n\
\n\
User must specify either: one or more data files, sock:[hostname[:port], or\n\
a begin time and a $PROJECT environment variable.\\n\
\n\
Examples:\n" <<
	argv0 << "" << '\n' <<
	argv0 << "" << '\n' <<
	argv0 << "" << '\n' <<
	argv0 << "" << endl;
    return 1;
}

/* static */
bool DataPrep::interrupted = false;

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

	auto_ptr<Resampler> resampler;

	auto_ptr<SortedSampleInputStream> sis;

	vector<const Variable*> variables;


	set<DSMSensor*> activeSensors;
        set<const DSMConfig*> activeDsms;

	if (dataFileNames.size() == 0) {

	    // user has specified a list of variables
            // $PROJECT env var and a start and end time.
            // Get the project config corresponding to
            // the times.
	    //
	    // input
	    //	1. list of variable names
	    //	2. start and end time
	    //	3. $ISFF and $PROJECT
	    // products:
	    //  0. project, from $ISFF,$PROJECT, start, end, ProjectConfigs
	    //	1. vector of matched Variable*s
	    //	2. set of dsms, from project and variable list
	    // 		we may want the dsms in order to figure
	    //		out the archive fileset, in case
	    //		the data has not been merged.
	    //	3. set of sensors, from project and variable list
	    //  4. iochan, SortedSampleInputStream

            string configXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";
	    project.reset(ProjectConfigs::getProject(configXML,startTime));

	    // match the requested variables.
	    // on a match:
	    //	1. save the var to pass to the resampler
	    //  2. save the sensor in a set
	    //  3. save the dsm in a set
	    //  later:
	    //  1. get the fileset, create the SortedSampleInputStream
	    //	2. add the resampler as a processedSampleClient of the
	    //		stream and sensor
	    //	3. init the sensor

            variables = matchVariables(project.get(),activeDsms,activeSensors);

	    // now look for the files.
	    FileSet* fset = 0;
            list<FileSet*> fsets =
                project->findSampleOutputStreamFileSets();
            if (fsets.size() == 0 && activeDsms.size() == 1) {
                const DSMConfig* dsm = *(activeDsms.begin());
	    	fsets =
                    project->findSampleOutputStreamFileSets(dsm->getName());
                if (fsets.size() == 0)
                    throw n_u::InvalidParameterException(
                        progname,"cannot find fileset for dsm",dsm->getName());
            }
            if (fsets.size() == 0) 
                throw n_u::InvalidParameterException(progname,
                    "cannot find fileset","");
            fset = fsets.front();
            fset->setStartTime(startTime);
            fset->setEndTime(endTime);
            iochan = fset;
	}
	else {
	    // user has specified one or more files

	    // input
	    //	1. list of variable names
	    //	2. list of file names
	    // products:
	    //  0. project, from header of first file
	    //	1. vector of variables
	    //	2. set of dsms
	    //	3. set of sensors

	    FileSet* fset = new nidas::dynld::FileSet();
	    list<string>::const_iterator fi;
	    for (fi = dataFileNames.begin(); fi != dataFileNames.end(); ++fi)
		  fset->addFileName(*fi);

	    // read the first header to get the project configuration
	    // name
	    sis.reset(new SortedSampleInputStream(fset));
            sis->setHeapBlock(true);
	    sis->init();
	    sis->readHeader();
	    SampleInputHeader header = sis->getHeader();

	    // parse the config file.
	    xmlFileName = header.getConfigName();
	    xmlFileName = Project::expandEnvVars(xmlFileName);
	    auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(xmlFileName));

	    project.reset(Project::getInstance());
	    project->fromDOMElement(doc->getDocumentElement());

	    list<Variable*>::const_iterator rvi = reqVars.begin();

	    // match the variables.
	    // on a match:
	    //	1. save the var to pass to the resampler
	    //  2. save the sensor in a set
	    //  3. save the dsm in a set
	    // later:
	    //	1. init the sensors
	    //	2. add the resampler as a processedSampleClient of the sensor
            variables = matchVariables(project.get(),activeDsms,activeSensors);
	}

	set<DSMSensor*>::const_iterator si = activeSensors.begin();
	for ( ; si != activeSensors.end(); ++si) {
	    DSMSensor* sensor = *si;
            SampleTagIterator ti = sensor->getSampleTagIterator();
            for ( ; ti.hasNext(); ) {
                const SampleTag* tag = ti.next();
                iochan->addSampleTag(tag);
            }
        }

        iochan = iochan->connect();
        sis.reset(new SortedSampleInputStream(iochan));
        sis->setHeapBlock(true);


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

	si = activeSensors.begin();
	for ( ; si != activeSensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->init();
            SampleTagIterator ti = sensor->getSampleTagIterator();
            for ( ; ti.hasNext(); ) {
                const SampleTag* tag = ti.next();
                sis->addSampleTag(tag);
            }
	    // sis->addProcessedSampleClient(resampler.get(),sensor);
	}

        sis->init();
        resampler->connect(sis.get());

	DumpClient dumper(format,cout);

	resampler->addSampleClient(&dumper);

        if (startTime.toUsecs() != 0) {
            cerr << "Searching for time " <<
                startTime.format(true,"%Y %m %d %H:%M:%S");
            sis->search(startTime);
            cerr << " done." << endl;
            dumper.setStartTime(startTime);
        }
        if (endTime.toUsecs() != 0) dumper.setEndTime(endTime);

	dumper.printHeader(variables);

	for (;;) {
	    sis->readSamples();
	    if (interrupted) break;
	}
    }
    catch (nidas::core::XMLException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::EOFException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::IOException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
	cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return DataPrep::main(argc,argv);
}
