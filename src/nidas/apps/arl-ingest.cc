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
#include <nidas/core/NidasApp.h>


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
        header(),
        _app("arl-ingest")
         {
    }

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

    /**
	 * main is the entry point for the ARLIngest class.  argc and argv are taken from 
	 * the normal places.
	*/
    static int main(int argc, char** argv) throw();

    /**
	 * usage emits a generic usage message to std::cout
	*/
    int usage(const char* argv0);

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

    list<string> inputFileNames; //input files
    string outputFileName; //output sample file
    int outputFileLength, dsmid, spsid;
    double leapSeconds;
    SampleInputHeader header;

    NidasApp _app;
};


int main(int argc, char** argv) {
    return ARLIngest::main(argc,argv);
}

/* main entry point */
int ARLIngest::main(int argc, char** argv) throw() {
    NidasApp::setupSignals();
    ARLIngest ingest;
    int res = ingest.parseRunstring(argc,argv);
    if (res != 0) {
        return res;
    }
    return ingest.run();
}


/* static */
int ARLIngest::usage(const char* argv0) {
    cerr << argv0 << " - A tool to convert raw ARL Sonic Data data records to the NIDAS data format" << endl << endl;
    cerr << "Usage: " << argv0 << "-x <xml> -d <dsm> -e <height> -o <output> <list of input files>\n" << endl;
    cerr << endl;
    cerr << endl;
    cerr << "Standard nidas options:" << endl << _app.usage();
    cerr << argv0 << " additional options:" << endl;
    cerr << "-d <dsm-name>" << endl;
    cerr << "The human readable name of the DSM the ARL sonde is connected to. Usually this"<< endl;
    cerr << "is the same as the tower site name (eg 'tnw12') but not always. This will be"<< endl;
    cerr << "checked against the configuration, which is required" << endl << endl;
    cerr << "-e <height>" << endl;
    cerr << "Inform this tool the data sets to be ingested are associated with this height"<< endl;
    cerr << "level. Levels are defined in the XML config, but should be something like '10m',"<< endl;
    cerr << "'50m' or '1km'.  This is checked against the configuration, which is required" << endl;
    cerr << endl;
    exit(1);
    return 1;
}

int ARLIngest::parseRunstring(int argc, char** argv) throw() {
    NidasApp& app = _app;
    app.enableArguments(app.XmlHeaderFile | app.OutputFiles |
                        app.loggingArgs() |
                        app.Version | app.Help);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = false;

    vector<string> args(argv, argv+argc);
    app.parseArguments(args);
    if (app.helpRequested()) {
        usage(argv[0]);
    }

    string dsmName, height;
    extern char *optarg; //set by getopt
    extern int   optind; //"
    NidasAppArgv left(args);
    argc = left.argc;
    argv = left.argv;
    int opt_char; /* option character */
    while ((opt_char = getopt(left.argc, left.argv, "d:e:")) != -1) {
        switch (opt_char) {
        case 'd': dsmName = string(optarg); break;
        case 'e': height = string(optarg); break;
        case '?':
        default:
            break;
        }
    }

    if (optind < argc)
        while (optind < argc)
        	inputFileNames.push_back(argv[optind++]);

    outputFileName = app.outputFileName();
    outputFileLength = app.outputFileLength();
    cout << "out file name" <<  outputFileName << " length" << outputFileLength << " dsm name" << dsmName << " height" << height << endl;

    //rudimentary checking for proper arguments
    if ( inputFileNames.size() == 0 || outputFileName.size() == 0 || outputFileLength < 0) {
        cout << "Not enough, or bad args provided. See --help" << endl;
        return -1;
    }
    

    //throws error if missing needed parameters
    //sets dsmid and spsid per config file    
    prepareHeaderIds(app.xmlHeaderFile(), dsmName, height);     
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
        if (_app.interrupted()) break;
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
