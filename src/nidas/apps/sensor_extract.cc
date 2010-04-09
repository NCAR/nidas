/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/apps/nidsmerge.cc $

    Extract samples from a list of sensors from an archive.

 ********************************************************************
*/

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/RawSampleOutputStream.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>

#include <csignal>
#include <climits>

#include <iomanip>

// hack for arm-linux-gcc from Arcom which doesn't define LLONG_MAX
// #ifndef LLONG_MAX
// #   define LLONG_MAX    9223372036854775807LL
// #   define LLONG_MIN    (-LLONG_MAX - 1LL)
// #endif

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class SensorExtract: public HeaderSource
{
public:

    SensorExtract();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    void sendHeader(dsm_time_t thead,SampleOutput* out)
        throw(n_u::IOException);
    
    /**
     * for debugging.
     */
    void printHeader();

private:

    static bool interrupted;

    list<string> inputFileNames;

    auto_ptr<n_u::SocketAddress> sockAddr;

    string outputFileName;

    int outputFileLength;

    SampleInputHeader header;

    set<dsm_sample_id_t> includeIds;

    set<dsm_sample_id_t> excludeIds;

    map<dsm_sample_id_t,dsm_sample_id_t> newids;

};

int main(int argc, char** argv)
{
    n_u::LogConfig lc;
    lc.level = n_u::LOGGER_INFO;
    n_u::Logger::getInstance()->setScheme
          (n_u::LogScheme("sensor_extract").addConfig (lc));
    return SensorExtract::main(argc,argv);
}


/* static */
bool SensorExtract::interrupted = false;

/* static */
void SensorExtract::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SensorExtract::interrupted = true;
    break;
    }
}

/* static */
void SensorExtract::setupSignals()
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
    act.sa_sigaction = SensorExtract::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int SensorExtract::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-s dsmid,sensorid[,newdsmid,newsensorid]] [-s dsmid,sensorid ...]\n\
	[-x dsmid,sensorid] [-l output_file_length] output input ... \n\n\
    -s dsmid,sensorid: the dsm id and sensor id of samples to extract\n\
            more than one -s option can be specified\n\
	    newdsm,newsensor: change id to newdsmid,newsensorid\n\
    -x dsmid,sensorid: the dsm id and sensor id of samples to exclude\n\
            more than one -x option can be specified\n\
	    either -s or -x options can be specified, but not both\n\
    -l output_file_length: length of output files, in seconds\n\
    output: output file name or file name format\n\
    input ...: one or more input file name or file name formats, or\n\
        sock:[hostname:port]  to connect to a socket on hostname, or\n\
            hostname defaults to \"localhost\", port defaults to " <<
                NIDAS_RAW_DATA_PORT_TCP << "\n\
        unix:path to connect to a unix socket on the localhost\n\
        \n\
" << endl;
    return 1;
}

/* static */
int SensorExtract::main(int argc, char** argv) throw()
{
    setupSignals();

    SensorExtract merge;

    int res;
    
    if ((res = merge.parseRunstring(argc,argv)) != 0) return res;

    return merge.run();
}


SensorExtract::SensorExtract():
	outputFileLength(0)
{
}

int SensorExtract::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "l:s:x:")) != -1) {
	switch (opt_char) {
	case 'l':
	    outputFileLength = atoi(optarg);
	    break;
        case 's':
            {
                unsigned int dsmid;
                unsigned int sensorid;
                unsigned int newdsmid;
                unsigned int newsensorid;
                int i;
                i = sscanf(optarg,"%d,%d,%d,%d",
                    &dsmid,&sensorid,&newdsmid,&newsensorid);
                if (i < 2) return usage(argv[0]);
                dsm_sample_id_t id = 0;
                id = SET_DSM_ID(id,dsmid);
                id = SET_SHORT_ID(id,sensorid);
                includeIds.insert(id);
                if (i < 3) newdsmid = dsmid;
                if (i < 4) newsensorid = sensorid;
                dsm_sample_id_t newid = 0;
                newid = SET_DSM_ID(newid,newdsmid);
                newid = SET_SHORT_ID(newid,newsensorid);
                newids[id] = newid;
            }
            break;
        case 'x':
            {
                unsigned int dsmid;
                unsigned int sensorid;
                int i;
                i = sscanf(optarg,"%d,%d",&dsmid,&sensorid);
                if (i < 2) return usage(argv[0]);
                dsm_sample_id_t id = 0;
                id = SET_DSM_ID(id,dsmid);
                id = SET_SHORT_ID(id,sensorid);
                excludeIds.insert(id);
            }
            break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind < argc) outputFileName = argv[optind++];
    for ( ;optind < argc; )
        inputFileNames.push_back(argv[optind++]);
    if (inputFileNames.size() == 0) return usage(argv[0]);

    if (inputFileNames.size() == 1) {
        string url = inputFileNames.front();
        if (url.substr(0,5) == "sock:") {
            url = url.substr(5);
	    string hostName = "127.0.0.1";
            int port = NIDAS_RAW_DATA_PORT_TCP;
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
	else if (url.substr(0,5) == "unix:") {
	    url = url.substr(5);
            sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
    }
    if (includeIds.size() + excludeIds.size() == 0) return usage(argv[0]);
    if (includeIds.size() > 0 &&  excludeIds.size() > 0) return usage(argv[0]);
    return 0;
}

void SensorExtract::sendHeader(dsm_time_t thead,SampleOutput* out)
    throw(n_u::IOException)
{
    printHeader();
    header.write(out);
}

void SensorExtract::printHeader()
{
    cerr << "ArchiveVersion:" << header.getArchiveVersion() << endl;
    cerr << "SoftwareVersion:" << header.getSoftwareVersion() << endl;
    cerr << "ProjectName:" << header.getProjectName() << endl;
    cerr << "SystemName:" << header.getSystemName() << endl;
    cerr << "ConfigName:" << header.getConfigName() << endl;
    cerr << "ConfigVersion:" << header.getConfigVersion() << endl;
}

int SensorExtract::run() throw()
{

    try {
	nidas::core::FileSet* outSet = new nidas::core::FileSet();
	outSet->setFileName(outputFileName);
	outSet->setFileLengthSecs(outputFileLength);

        SampleOutputStream outStream(outSet);
        outStream.setHeaderSource(this);

        IOChannel* iochan = 0;

        if (sockAddr.get()) {

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
            iochan = 
                nidas::core::FileSet::getFileSet(inputFileNames);
        }

        // RawSampleInputStream owns the iochan ptr.
        RawSampleInputStream input(iochan);

        input.setMaxSampleLength(32768);

        input.readInputHeader();
        // save header for later writing to output
        header = input.getInputHeader();

        n_u::UTime screenTime(true,2001,1,1,0,0,0,0);

        try {
            for (;;) {

                Sample* samp = input.readSample();
                if (interrupted) break;

                if (samp->getTimeTag() < screenTime.toUsecs()) continue;

                dsm_sample_id_t id = samp->getId();

		if (includeIds.size() > 0) {
		    if (includeIds.find(id) != includeIds.end()) {
			dsm_sample_id_t newid = newids[id];
			samp->setId(newid);
			outStream.receive(samp);
		    }
		}
		else if (excludeIds.find(id) == excludeIds.end())
			outStream.receive(samp);
                samp->freeReference();
            }
        }
        catch (n_u::EOFException& ioe) {
            cerr << ioe.what() << endl;
        }

	outStream.finish();
	outStream.close();
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}

