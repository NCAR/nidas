// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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

#include <nidas/core/FileSet.h>
#include <nidas/core/Bzip2FileSet.h>
#include <nidas/core/Project.h>
#include <nidas/core/Version.h>
#include <nidas/core/DSMConfig.h>
 #include <nidas/core/DSMSensor.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/SortedSampleSet.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Exception.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/IOException.h>


#include <unistd.h>
#include <getopt.h>

#include <csignal>
#include <climits>
 #include <sys/stat.h>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class ARLIngest: public HeaderSource
{
public:

    ARLIngest():
        inputFileNames(),
        outputFileName(),
        outputFileLength(0),
        dsmid(0),
        spsid(0),
        leapSeconds(0.0),
        header() {
        // HeaderSource::setDefaults(header);
    }

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

	/**
	 *sigAction is the callback when a signals are captured
	*/
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);
    /**
	 *setupSignals registers the sigAction callback
	*/
    static void setupSignals();
    /**
	 * main is the entry point for the ARLIngest class.  argc and argv are taken from 
	 * the normal places.
	*/
    static int main(int argc, char** argv) throw();

    /**
	 * usage emits a generic usage message to std::cout
	*/
    static int usage(const char* argv0);

    /**
	 * sendHeader does something I still dont understand
	*/
    void sendHeader(dsm_time_t thead,SampleOutput* out) throw(n_u::IOException);
    
    /**
     * printHeader dumps out header information (for debugging)
     */
    void printHeader();

private:
	/**
	 * arl_ingest_one ingests a single file by the passed filename and writes the re-encoded data into the passed SampleOutputStream
	 */
	bool arl_ingest_one(SampleOutputStream&,  string) throw(); 
	/**
	* writeLine writes a single sample line to the passed output stream applying the needed offset UTime
	*/
	void writeLine(SampleOutputStream &, string &, n_u::UTime);
    void prepareHeaderIds(string xmlfilename, string dsmName, string height) throw(n_u::Exception);

    static bool interrupted; //catch those pesky Ctrl- actions
    list<string> inputFileNames; //input files
    string outputFileName; //output sample file
    int outputFileLength, dsmid, spsid;
    double leapSeconds;
    SampleInputHeader header;
};


int main(int argc, char** argv) {
    return ARLIngest::main(argc,argv);
}


/* static */
bool ARLIngest::interrupted = false;

/* static */
void ARLIngest::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    "received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            ARLIngest::interrupted = true;
    break;
    }
}

/* static */
void ARLIngest::setupSignals()
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
    act.sa_sigaction = ARLIngest::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int ARLIngest::usage(const char* argv0) {
    cerr << argv0 << " - A tool to convert raw ARL Sonic Data data records to the NIDAS data format\n\n" << \
    "Usage: " << argv0 << " [-l <length>] -x <xml> -d <dsm> -e <height> -o <output> <list of input files>\n" << \
    "\n" << \
    "\n" << \
    "Flag & Args   Definition" << \
  	"-h|--help   : Show this message\n" << \
    "-x:--xml    : File path to project XML file (eg $ISFF/projects/$PROJECT/\n" <<\
    "              ISFF/config/$PROJECT.xml). The DSM and Sample ID will be defered\n" <<\
    "              from a combination of this XML file, the dsm name (keep reading),\n" <<\
    "              and height parameters.  Enviromental vars will be expanded\n" <<\
    "              internally, so per convention, it would be best to escape\n" <<\
    "              the $ character\n" << \
  	"-o|--output : nidsmerge compat output file (NIDAS Sample file)\n" << \
  	"-l|--length : output file length (per NIDAS rules)\n" << \
    "-d|--dsm    : The human readable name of the DSM the ARL sonde is connected to.\n" <<\
    "              Usually this is the same as the tower site name (eg 'tnw12') but\n" <<\
    "              not always. This will be checked against the configuration\n" << \
    "-e|--height : Inform this tool the data sets to be ingested are associated\n" <<\
    "              with this height level. Levels are defined in the XML config,\n" <<\
    "              but should be something like '10m', '50m' or '1km'.\n" << \
 	endl;
    return 1;
}

/* static */
int ARLIngest::main(int argc, char** argv) throw() {
    setupSignals();
    ARLIngest ingest;
    int res = ingest.parseRunstring(argc,argv);
    if (res != 0) {
    	return res;
    }
    return ingest.run();
}

int ARLIngest::parseRunstring(int argc, char** argv) throw() {
	int c = 0;
    string dsmName, height, configName;;
	while (1) {
	int option_index = 0;
	static struct option long_options[] = {
	    {"help",   no_argument,       0, 'h'},
	    {"xml",    required_argument, 0, 'x'},
	    {"output", required_argument, 0, 'o'},
	    {"length", required_argument, 0, 'l'},
	   	{"dsm",    required_argument, 0, 'd'},
	   	{"height", required_argument, 0, 'e'},
	    {NULL, 0, NULL, 0}
	};

	c = getopt_long(argc, argv, "hx:o:l:d:e:", long_options, &option_index);
	if (c == -1) break;

	switch (c) {
        case 'h':
            usage(argv[0]);
            return 0;
            break;
		case 'x': configName = string(optarg); break;
		case 'o': outputFileName = string(optarg); break;
		case 'l': outputFileLength = atoi(optarg); break;
	    // case 'd': dsmid = atoi(optarg); break;
   		// case 's': spsid = atoi(optarg); break;
        case 'd': dsmName = string(optarg); break;
        case 'e': height = string(optarg); break;
		case '?':
		default:
		    // printf("?? getopt returned character code 0%o ??\n", c);
			break;
		}
	}

    if (optind < argc)
        while (optind < argc)
        	inputFileNames.push_back(argv[optind++]);

    //rudimentary checking for proper arguments
    if ( inputFileNames.size() == 0 || outputFileName.size() == 0 || outputFileLength < 0) {
        cout << "Not enough, or bad args provided. See --help" << endl;
        return -1;
    }

    //throws error if missing needed parameters
    //sets dsmid and spsid per config file
    prepareHeaderIds(configName, dsmName, height); 
    
    return 0;
}

/**
* prepareHeaderIds parses the xml files pointed to by xmlfilename and using the
* passed dsm name and height, modifies dsmid and spsid to the proper values.  It throws
* exceptions on file IO errors, or when no configuration match is found. Additionally,
* it loads proper values into the stored header which is attached to samples when
* sendHeader is called
*/
void ARLIngest::prepareHeaderIds(string xmlfilename, string dsmName, string height)  throw(n_u::Exception){
    //easy peasy
    header.setArchiveVersion(Version::getArchiveVersion());
    header.setSoftwareVersion(Version::getSoftwareVersion());
    header.setConfigName(xmlfilename);

    struct stat statbuf;
    xmlfilename = n_u::Process::expandEnvVars(xmlfilename);
    
    //a weeee  bit harder
    if (::stat(xmlfilename.c_str(),&statbuf) == 0) {
        n_u::auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(xmlfilename));
        static ::Project* project = Project::getInstance();
        project->fromDOMElement(doc->getDocumentElement());
        header.setProjectName (project->getName());
        header.setSystemName(project->getSystemName());
        header.setConfigVersion (project->getConfigVersion());

        //iterate and locate the DSM id by site and sensor id by sensor height tag
        DSMConfigIterator di = project->getDSMConfigIterator();
        for ( ; di.hasNext(); ) {
            const DSMConfig* dsm = di.next();
            if (dsm->getName() == dsmName) {
                dsmid = dsm->getId(); //keep hunting for the correct height
                SensorIterator si = dsm->getSensorIterator();
                for ( ; si.hasNext(); ) {
                    const DSMSensor* sensor = si.next();
                    if (sensor->getHeightString() == height) {
                        spsid = sensor->getSensorId();
                    }
                }
            }
        }
    } else {
        cerr << "File '" << xmlfilename << "' does not exist.  Cannot parse required configuration" << endl;
        exit(-1);
    }
    if (dsmid == 0) {
        cerr << "Unable to find a DSM configuration named '" << dsmName << "' in '" << xmlfilename << "'" << endl;
        exit(-1);
    }
    if (spsid == 0) {
        cerr << "Unable to find sensor at named height '" << height << "' attached to DSM '" << dsmName << "' in '" << xmlfilename << "'" << endl;
        exit(-1);
    }
    cout << "Found DSM with Id:" << dsmid << " and Sensor Id:" << spsid << endl;
    // printHeader();

} 

/*sendHeader conforms to nidas::core::HeaderSource interface and should write useful pieces of
metadata to the output header stream*/
void ARLIngest::sendHeader(dsm_time_t,SampleOutput* out) throw(n_u::IOException) {
    header.write(out);
}

void ARLIngest::printHeader() {
    cerr << "ArchiveVersion:" << header.getArchiveVersion() << endl;
    cerr << "SoftwareVersion:" << header.getSoftwareVersion() << endl;
    cerr << "ProjectName:" << header.getProjectName() << endl;
    cerr << "SystemName:" << header.getSystemName() << endl;
    cerr << "ConfigName:" << header.getConfigName() << endl;
    cerr << "ConfigVersion:" << header.getConfigVersion() << endl;
}

int ARLIngest::run() throw() {
    // cout << "Using UTC - Sample Time leap second offset of "
    //      << leapSeconds << endl;
    try {
    	nidas::core::FileSet* outSet = 0;
        if (outputFileName.find(".bz2") != string::npos)
        {
#ifdef HAVE_BZLIB_H
            outSet = new nidas::core::Bzip2FileSet();
#else
            cerr << "bz2 output not supported by this software build." << endl;
            return 1;
#endif
        }
        else {
            outSet = new nidas::core::FileSet();
        }
        outSet->setFileName(outputFileName);
        outSet->setFileLengthSecs(outputFileLength);

        SampleOutputStream outStream(outSet);
        outStream.setHeaderSource(this);
        
        //iterate through files ingesting each file
        while (!inputFileNames.empty() && arl_ingest_one(outStream, inputFileNames.front())) {
            inputFileNames.pop_front();
        }
        outStream.flush();
        outStream.close();
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
        return 1;
    }
    return 0;
}

bool ARLIngest::arl_ingest_one(SampleOutputStream &sout, string filename) throw() {
	string line; 
	n_u::UTime start; //start time tag

	ifstream datfile(filename.c_str());
	if (!datfile.is_open()) {
		cerr << "Unable to open " << filename << " for reading" << endl;
		return false;
	}
	
	//beginning line should look something like "20160711, 1800,  01, 02"
	std::getline(datfile, line);
	if (line.size() < 22) {
		datfile.close();
		cerr << "File '" << filename << "' has invalid data header '" << line << "'" << endl;
		return false;
	}
	
	try {
		start = n_u::UTime::parse(true, line.substr(0, 14), "%Y%m%d, %H%M");
	} catch (n_u::IOException& ioe) {
		datfile.close();
		cerr << "File '" << filename << "' time is badly formed in header '" << line << "'" << endl;
		return false;
	}
	cout << filename << " begins at " << start << endl;
	start = n_u::UTime(start.toDoubleSecs() - leapSeconds); // apply any known leapsecond correction
	writeLine(sout, line, start);

	//extract tower ID and height ??
	while (std::getline(datfile,line)) {
		//lines nominally look like "00.011,A,+000.03,+000.00,-000.01,M,+348.85,+028.97,00,+2.4225,+2.4225,+0.6750,+0.3050,39"
		size_t comma = line.find_first_of(',');
		
		if (comma == std::string::npos || line.length()  == 0) {
			cerr << "Line does not have valid formatting and is being ignored " << line << endl;
			continue; 
		}
		try {
			n_u::UTime offset(atof(line.substr(0, comma).c_str()) + start.toDoubleSecs());
			writeLine(sout, line, offset);
		} catch (n_u::IOException& ioe) {
			cerr << "Unable to parse time from " << line.substr(0, comma) << endl;
		}
	}
	datfile.close();
	return true;
}

void ARLIngest::writeLine(SampleOutputStream &sout, string &line, n_u::UTime time) {
	line += "\n"; //add newline back in
	SampleT<char>* samp = getSample<char>(line.length()+1);
	samp->setDSMId(dsmid);
	samp->setSpSId(spsid);
	samp->setTimeTag(time.toUsecs());
	memcpy(samp->getDataPtr(), line.c_str(), line.length()+1); //copy up to and including null
	sout.receive(samp);
}
