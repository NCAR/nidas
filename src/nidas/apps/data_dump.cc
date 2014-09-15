// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

// #define _XOPEN_SOURCE	/* glibc2 needs this */

#include <ctime>

#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/core/IOChannel.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/Project.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/dynld/raf/IRIGSensor.h>
#include <nidas/util/Logger.h>
#include <nidas/util/Process.h>
#include <nidas/util/util.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/core/NidasApp.h>

#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { DEFAULT, ASCII, HEX_FMT, SIGNED_SHORT, UNSIGNED_SHORT,
                          FLOAT, IRIG, INT32, ASCII_7, NAKED } format_t;

    typedef enum idfmt {DECIMAL, HEX_ID, OCTAL } id_format_t;

    DumpClient(set<dsm_sample_id_t>,format_t,ostream&,id_format_t idfmt);

    virtual ~DumpClient() {}

    void flush() throw() {}

    bool receive(const Sample* samp) throw();

    void printHeader();

    DumpClient::format_t typeToFormat(sampleType t);

private:

    set<dsm_sample_id_t> sampleIds;

    bool allDSMs;

    bool allSensors;

    format_t format;

    ostream& ostr;

    const n_u::EndianConverter* fromLittle;

    enum idfmt _idFormat;

    DumpClient(const DumpClient&);
    DumpClient& operator=(const DumpClient&);
};


DumpClient::DumpClient(set<dsm_sample_id_t> ids,format_t fmt,ostream &outstr,id_format_t idfmt):
        sampleIds(ids),allDSMs(false),allSensors(false),
	format(fmt),ostr(outstr),
        fromLittle(n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN)),
        _idFormat(idfmt)
{
    if (sampleIds.size() > 0) {
        dsm_sample_id_t sampleId = *sampleIds.begin();
        if(GET_DSM_ID(sampleId) == 1023) allDSMs = true;
        if(sampleIds.size() == 1 && GET_SHORT_ID(sampleId) == 65535) allSensors = true;
    }
}

void DumpClient::printHeader()
{
    cout << "|--- date time --------|  deltaT";
    if (allDSMs || allSensors || sampleIds.size() > 1)
        cout << "   id   ";
    cout << "       len data..." << endl;
}


/*
 * This function is not as useful as it seems. Currently in NIDAS,
 * all raw samples from sensors are of type CHAR_ST, and processed samples
 * are FLOAT_ST. So this function does not automagically result in raw
 * data being displayed in its natural format.
 */
DumpClient::format_t
DumpClient::
typeToFormat(sampleType t)
{
  static std::map<sampleType,DumpClient::format_t> themap;
  if (themap.begin() == themap.end())
  {
    themap[CHAR_ST] = ASCII;
    themap[UCHAR_ST] = HEX_FMT;
    themap[SHORT_ST] = SIGNED_SHORT;
    themap[USHORT_ST] = UNSIGNED_SHORT;
    themap[INT32_ST] = INT32;
    themap[UINT32_ST] = HEX_FMT;
    themap[FLOAT_ST] = FLOAT;
    themap[DOUBLE_ST] = FLOAT;
    themap[INT64_ST] = HEX_FMT;
    themap[UNKNOWN_ST] = HEX_FMT;
  }
  return themap[t];
}


bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    static dsm_time_t prev_tt = 0;

    dsm_sample_id_t sampid = samp->getId();
    DLOG(("sampid=") << samp->getDSMId() << ',' << samp->getSpSId());

    unsigned int dsmid = GET_DSM_ID(sampid);
    unsigned int spsid = GET_SPS_ID(sampid);

    if (allDSMs) {
        if (!allSensors) {
            set<dsm_sample_id_t>::const_iterator si =  sampleIds.begin();
            for ( ; si != sampleIds.end(); ++si)
                if (GET_SPS_ID(*si) == spsid) break;
            if (si == sampleIds.end()) return false;
        }
    }
    else {
        if (allSensors) {
            set<dsm_sample_id_t>::const_iterator si =  sampleIds.begin();
            for ( ; si != sampleIds.end(); ++si)
                if (GET_DSM_ID(*si) == dsmid) break;
            if (si == sampleIds.end()) return false;
        }
        else if (sampleIds.find(sampid) == sampleIds.end()) return false;
    }

    // Format the line leader into a separate string before handling the
    // chosen output format, in case the output format is naked.
    ostringstream leader;

    leader << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%4f") << ' ';

    leader << setprecision(4) << setfill(' ');
    if (prev_tt != 0) {
        double tdiff = (tt - prev_tt) / (double)(USECS_PER_SEC);
        leader << setw(7) << tdiff << ' ';
    }
    else leader << setw(7) << 0 << ' ';

    if (allDSMs || allSensors || sampleIds.size() > 1) {
        leader << setw(2) << setfill(' ') << samp->getDSMId() << ',';
        switch(_idFormat) {
        case HEX_ID:
            leader << "0x" << setw(4) << setfill('0') << hex << samp->getSpSId() << dec << ' ';
            break;
#ifdef SUPPORT_OCTAL_IDS
        case OCTAL:
            leader << "0" << setw(6) << setfill('0') << oct << samp->getSpSId() << dec << ' ';
            break;
#else
        default:
#endif
        case DECIMAL:
            leader << setw(4) << samp->getSpSId() << ' ';
            break;
        }
    }

    leader << setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';
    prev_tt = tt;

    format_t sample_format = format;

    // Naked format trumps everything, otherwise force floating point
    // samples to be printed in FLOAT format.
    if (format != NAKED) {
        if (samp->getType() == FLOAT_ST) sample_format = FLOAT;
        else if (samp->getType() == DOUBLE_ST) sample_format = FLOAT;
        else if (format == DEFAULT)
        {
            sample_format = typeToFormat(samp->getType());
        }
        ostr << leader.str();
    }

    switch(sample_format) {
    case ASCII:
    case ASCII_7:
	{
        const char* cp = (const char*)samp->getConstVoidDataPtr();
        size_t l = samp->getDataByteLength();
        if (l > 0 && cp[l-1] == '\0') l--;  // exclude trailing '\0'
        if (sample_format ==  ASCII_7) {
            char cp7[l];
            char* xp;
            for (xp=cp7; *cp; ) *xp++ = *cp++ & 0x7f;
            ostr << n_u::addBackslashSequences(string(cp7,l)) << endl;
        }
        else {
            ostr << n_u::addBackslashSequences(string(cp,l)) << endl;
        }
        }
        break;
    case HEX_FMT:
        {
	const unsigned char* cp =
		(const unsigned char*) samp->getConstVoidDataPtr();
	ostr << setfill('0');
	for (unsigned int i = 0; i < samp->getDataByteLength(); i++)
	    ostr << hex << setw(2) << (unsigned int)cp[i] << dec << ' ';
	ostr << endl;
	}
        break;
    case SIGNED_SHORT:
	{
	const short* sp =
		(const short*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(short); i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case UNSIGNED_SHORT:
	{
	const unsigned short* sp =
		(const unsigned short*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(short); i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case FLOAT:
         if (samp->getType() == DOUBLE_ST) ostr << setprecision(10);
         else ostr << setprecision(5);

         ostr << setfill(' ');

        for (unsigned int i = 0; i < samp->getDataLength(); i++)
            ostr << setw(10) << samp->getDataValue(i) << ' ';
        ostr << endl;
        break;
    case IRIG:
	{
	const unsigned char* dp = (const unsigned char*) samp->getConstVoidDataPtr();
	unsigned int nbytes = samp->getDataByteLength();
	struct timeval32 tv;
	char timestr[128];
	struct tm tm;

        // IRIG time
        time_t irig_sec = fromLittle->int32Value(dp);
	dp += sizeof(tv.tv_sec);
        int irig_usec = fromLittle->int32Value(dp);
	dp += sizeof(tv.tv_usec);

	gmtime_r(&irig_sec,&tm);
	strftime(timestr,sizeof(timestr)-1,"%H:%M:%S",&tm);
	ostr << "irig: " << timestr << '.' << setw(6) << setfill('0') << irig_usec << ", ";

        if (nbytes >= 2 * sizeof(struct timeval32) + 1) {

            // UNIX time

            time_t unix_sec = fromLittle->int32Value(dp);
            dp += sizeof(tv.tv_sec);
            int unix_usec = fromLittle->int32Value(dp);
            dp += sizeof(tv.tv_usec);

            gmtime_r(&unix_sec,&tm);
            strftime(timestr,sizeof(timestr)-1,"%H:%M:%S",&tm);
            ostr << "unix: " << timestr << '.' << setw(6) << setfill('0') << unix_usec << ", ";
            ostr << "i-u: " << setfill(' ') << setw(4) << ((irig_sec - unix_sec) * USECS_PER_SEC +
                (irig_usec - unix_usec)) << " us, ";
        }

	unsigned char status = *dp++;

        ostr << "status: " << setw(2) << setfill('0') << hex << (int)status << dec <<
		'(' << IRIGSensor::shortStatusString(status) << ')';
        if (nbytes >= 2 * sizeof(struct timeval32) + 2)
            ostr << ", seq: " << (int)*dp++;
        if (nbytes >= 2 * sizeof(struct timeval32) + 3)
            ostr << ", synctgls: " << (int)*dp++;
        if (nbytes >= 2 * sizeof(struct timeval32) + 4)
            ostr << ", clksteps: " << (int)*dp++;
        if (nbytes >= 2 * sizeof(struct timeval32) + 5)
            ostr << ", maxbacklog: " << (int)*dp++;
	ostr << endl;
	}
        break;
    case INT32:
	{
	const int* lp =
		(const int*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(int); i++)
	    ostr << setw(8) << lp[i] << ' ';
	ostr << endl;
	}
        break;
    case NAKED:
        {
        // Write the raw sample unadorned and unformatted.
        // NIDAS adds a NULL char, '\0', if the user has specified
        // a separator that ends in \r or \n. In this way records are easily
        // scanned with sscanf without adding a NULL. We don't know
        // what the separator actually is, but it should be mostly
        // right to check for a ending "\n\0" or "\r\0" here, and if found,
        // remove the \0.
        size_t n = samp->getDataByteLength();
        const char* ptr = (const char*) samp->getConstVoidDataPtr(); 
        if (n > 1 && ptr[n-1] == '\0' && 
                (ptr[n-2] == '\r' || ptr[n-2] == '\n')) n--;
        ostr.write(ptr,n);
        }
    case DEFAULT:
        break;
    }
    return true;
}

class DataDump
{
public:

    DataDump();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    static int main(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

private:

    static const int DEFAULT_PORT = 30000;

    bool processData;

    string xmlFileName;

    list<string> dataFileNames;

    auto_ptr<n_u::SocketAddress> sockAddr;

    set<dsm_sample_id_t> sampleIds;

    DumpClient::format_t format;

    DumpClient::id_format_t idFormat;

};

DataDump::DataDump():
    processData(false),xmlFileName(),dataFileNames(),
    sockAddr(0), sampleIds(),
    format(DumpClient::DEFAULT),
    idFormat(DumpClient::DECIMAL)
{
}

int DataDump::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
    dsm_sample_id_t sampleId = 0;

    while ((opt_char = getopt(argc, argv, "Ad:FHi:Il:Lps:SUx:X7n")) != -1) {
	switch (opt_char) {
	case 'A':
	    format = DumpClient::ASCII;
	    break;
	case '7':
	    format = DumpClient::ASCII_7;
	    break;
	case 'd':
            cerr << "-d option is obsolete, use -i instead" << endl;
            return usage(argv[0]);
	case 'F':
	    format = DumpClient::FLOAT;
	    break;
	case 'H':
	    format = DumpClient::HEX_FMT;
	    break;
        case 'n':
            format = DumpClient::NAKED;
            break;
	case 'i':
            {
                int dsmid1,dsmid2;
                int snsid1,snsid2;
                string soptarg(optarg);
                string::size_type ic = soptarg.find(',');
                if (ic == string::npos) return usage(argv[0]);
                string dsmstr = soptarg.substr(0,ic);
                string snsstr = soptarg.substr(ic+1);
                if (dsmstr.length() > 1 && (ic = dsmstr.find('-',1)) != string::npos) {
                    dsmid1 = strtol(dsmstr.substr(0,ic).c_str(),0,0);
                    dsmid2 = strtol(dsmstr.substr(ic+1).c_str(),0,0);
                }
                else {
                    dsmid1 = dsmid2 = atoi(dsmstr.c_str());
                }
                if (snsstr.length() > 1 && (ic = snsstr.find('-',1)) != string::npos) {
                    // strtol handles hex in the form 0xXXXX
                    snsid1 = strtol(snsstr.substr(0,ic).c_str(),0,0);
                    snsid2 = strtol(snsstr.substr(ic+1).c_str(),0,0);
                }
                else {
                    snsid1 = snsid2 = strtol(snsstr.c_str(),0,0);
                }
                for (int did = dsmid1; did <= dsmid2; did++) {
                    sampleId = SET_DSM_ID(sampleId,did);
                    for (int sid = snsid1; sid <= snsid2; sid++) {
                        sampleId = SET_SHORT_ID(sampleId,sid);
                        sampleIds.insert(sampleId);
                    }
                }
                if (snsstr.find("0x",0) != string::npos) idFormat = DumpClient::HEX_ID;
                // I don't think OCTAL will be a useful format for sensor+sample id,
                // so we'll leave this ifdef'd out.
#ifdef SUPPORT_OCTAL_IDS
                else if (snsstr.find('0',0) == 0 && snsstr.length() > 1)
                    idFormat = DumpClient::OCTAL;
#endif
            }
	    break;
	case 'I':
	    format = DumpClient::IRIG;
	    break;
	case 'l':
            {
                n_u::LogConfig lc;
                lc.level = atoi(optarg);
                n_u::Logger::getInstance()->setScheme
                  (n_u::LogScheme("data_dump_command_line").addConfig (lc));
            }
	    break;
	case 'L':
	    format = DumpClient::INT32;
	    break;
	case 'p':
	    processData = true;
	    break;
	case 's':
            cerr << "-s option is obsolete, use -i instead" << endl;
            return usage(argv[0]);
	case 'S':
	    format = DumpClient::SIGNED_SHORT;
	    break;
	case 'U':
	    format = DumpClient::UNSIGNED_SHORT;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case 'X':
	    idFormat = DumpClient::HEX_ID;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    //    if (format == DumpClient::DEFAULT)
    //    	format = (processData ? DumpClient::FLOAT : DumpClient::HEX_FMT);

    vector<string> inputs;
    for ( ; optind < argc; optind++) inputs.push_back(argv[optind]);
    if (inputs.size() == 0) inputs.push_back("sock:localhost");

    for (unsigned int i = 0; i < inputs.size(); i++) {
        string url = inputs[i];
	if (url.length() > 5 && url.substr(0,5) == "sock:") {
	    url = url.substr(5);
	    size_t ic = url.find(':');
            int port = DEFAULT_PORT;
            string hostName = url.substr(0,ic);
	    if (ic < string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    return usage(argv[0]);
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
    if (dataFileNames.size() == 0 && !sockAddr.get()) return usage(argv[0]);

    if (sampleId == 0) return usage(argv[0]);
    return 0;
}

int DataDump::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-i d,s ...] [-l log_level] [-p] [-x xml_file] [-A | -7 | -F | -H | -n | -S | -X | -L ] [inputURL ...]\n\
    -i d,s : d is a dsm id or range of dsm ids separated by '-', or -1 for all.\n\
             s is a sample id or range of sample ids separated by '-', or -1 for all.\n\
               Sample ids can be specified in 0x hex format with a leading 0x, in which\n\
               case they will also be output in hex, as with the -X option.\n\
	Use data_stats program to see DSM ids and sample ids of data in a file.\n\
        More than one -i can be specified.\n\
    -p: process (optional). Display processed samples rather than raw samples.\n\
    -x xml_file (optional). The default value is read from the input data header.\n\
    -A: ASCII output of character data (for samples from a serial sensor)\n\
    -7: 7-bit ASCII output\n\
    -F: floating point output (typically for processed output)\n\
    -H: hex output (typically for raw output)\n\
    -n: naked output, unadorned samples written exactly as they were read,\n\
        useful for ascii serial data to be replayed through sensor_sim\n\
    -I: output of IRIG clock samples. Status of \"SYMPCS\" means sync, year,\n\
        major-time, PPS, code and esync are OK. Lower case letters indicate not OK.\n\
        sync and esync (extended status sync) are probably always equal\n\
    -L: ASCII output of signed 32 bit integers\n\
    -l log_level: 7=debug,6=info,5=notice,4=warn,3=err, default=6\n\
    -S: ASCII output of signed 16 bit integers (useful for samples from an A2D)\n\
    -X: print sample ids in hex format\n\
    If a format is specified, that format is used for all the samples, except\n\
    that a floating point format is always used for floating point samples.\n\
    Otherwise the format is chosen according to the type in the sample, so\n\
    it is possible to dump samples in different formats.  This is useful for\n\
    dumping both raw and processed samples.  (See example below.)\n\
    inputURL: data input(s).  One of the following:\n\
        sock:host[:port]          (Default port is " << DEFAULT_PORT << ")\n\
        unix:sockpath             unix socket name\n\
        path                      one or more file names\n\
        Default inputURL is \"sock:localhost\"\n\
\n\
Examples:\n\
Display IRIG data of sensor 100 on dsm 1 from sock:localhost:\n\
  " << argv0 << " -i 1,100 -I\n\
Display ASCII data of sensor 200, dsm 1 from sock:localhost:\n\
  " << argv0 << " -i 1,200 -A\n\
Display ASCII data from archive files:\n\
  " << argv0 << " -i 1,200 -A xxx.dat yyy.dat\n\
Hex dump of sensor ids 200 through 210 using configuration in ads3.xml:\n\
  " << argv0 << " -i 3,200-210 -H -x ads3.xml xxx.dat\n\
Display processed data of sample 1 of sensor 200:\n\
  " << argv0 << " -i 3,201 -p sock:hyper\n\
Display processed data of sample 1, sensor 200, from unix socket:\n\
  " << argv0 << " -i 3,201 -p unix:/tmp/dsm\n\
Display all raw and processed samples in their default format:\n\
  " << argv0 << " -i -1,-1 -p -x path/to/project.xml file.dat\n" << endl;
    return 1;
}

/* static */
int DataDump::main(int argc, char** argv)
{
    NidasApp napp("data_dump");
    napp.setApplicationInstance();
    napp.setupSignals();

    DataDump dump;

    int res;

    if ((res = dump.parseRunstring(argc,argv))) return res;

    return dump.run();
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int DataDump::run() throw()
{
    NidasApp& napp = *NidasApp::getApplicationInstance();

    try {
        AutoProject project;

	IOChannel* iochan = 0;

	if (dataFileNames.size() > 0) {
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(dataFileNames);
            iochan = fset->connect();
	}
	else {
	    n_u::Socket* sock = new n_u::Socket(*sockAddr.get());
            iochan = new nidas::core::Socket(sock);
	}

        // If you want to process data, get the raw stream
	SampleInputStream sis(iochan,processData);	// SampleStream now owns the iochan ptr.
        sis.setMaxSampleLength(32768);
	// sis.init();
	sis.readInputHeader();
	const SampleInputHeader& header = sis.getInputHeader();

	list<DSMSensor*> allsensors;

	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(),&statbuf) == 0 || processData) {
	    auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(xmlFileName));

	    Project::getInstance()->fromDOMElement(doc->getDocumentElement());

	    DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();

	    for ( ; di.hasNext(); ) {
		const DSMConfig* dsm = di.next();
		const list<DSMSensor*>& sensors = dsm->getSensors();
		allsensors.insert(allsensors.end(),sensors.begin(),
			sensors.end());
	    }
	}
        XMLImplementation::terminate();

        SamplePipeline pipeline;
        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(0);
        pipeline.setProcSorterLength(0);

	// Always add dumper as raw client, in case user wants to dump
	// both raw and processed samples.
	if (processData) {
	    list<DSMSensor*>::const_iterator si;
	    for (si = allsensors.begin(); si != allsensors.end(); ++si) {
		DSMSensor* sensor = *si;
		sensor->init();
                //  1. inform the SampleInputStream of what SampleTags to expect
                sis.addSampleTag(sensor->getRawSampleTag());
	    }
	}

        DumpClient dumper(sampleIds,format,cout,idFormat);

	if (processData) {
            // 2. connect the pipeline to the SampleInputStream.
            pipeline.connect(&sis);
            pipeline.getProcessedSampleSource()->addSampleClient(&dumper);
            // 3. connect the client to the pipeline
            pipeline.getRawSampleSource()->addSampleClient(&dumper);
        }
        else {
            sis.addSampleClient(&dumper);
        }

        if (format != DumpClient::NAKED)
            dumper.printHeader();

        try {
            for (;;) {
                sis.readSamples();
                if (napp.interrupted()) break;
            }
        }
        catch (n_u::EOFException& e) {
            cerr << e.what() << endl;
        }
        catch (n_u::IOException& e) {
            if (processData)
                pipeline.getProcessedSampleSource()->removeSampleClient(&dumper);
            else
                pipeline.getRawSampleSource()->removeSampleClient(&dumper);

            pipeline.disconnect(&sis);
            pipeline.interrupt();
            pipeline.join();
            sis.close();
            throw(e);
        }
	if (processData) {
            pipeline.disconnect(&sis);
            pipeline.flush();
            pipeline.getProcessedSampleSource()->removeSampleClient(&dumper);
            pipeline.getRawSampleSource()->removeSampleClient(&dumper);
        }
        else {
            sis.removeSampleClient(&dumper);
        }
        sis.close();
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception& e) {
	cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
	return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return DataDump::main(argc,argv);
}
