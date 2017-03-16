// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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
#include <nidas/util/auto_ptr.h>
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

    DumpClient(const SampleMatcher&, format_t, ostream&);

    virtual ~DumpClient() {}

    void flush() throw() {}

    bool receive(const Sample* samp) throw();

    void printHeader();

    DumpClient::format_t typeToFormat(sampleType t);

    void
    setWarningTime(float w)
    {
        warntime = w;
    }

private:

    SampleMatcher _samples;

    format_t format;

    ostream& ostr;

    const n_u::EndianConverter* fromLittle;

    float warntime;

    DumpClient(const DumpClient&);
    DumpClient& operator=(const DumpClient&);
};


DumpClient::DumpClient(const SampleMatcher& matcher,
                       format_t fmt,
                       ostream &outstr):
    _samples(matcher),
    format(fmt), ostr(outstr),
    fromLittle(n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN)),
    warntime(0.0)
{
}

void DumpClient::printHeader()
{
    cout << "|--- date time --------|  deltaT";
    if (!_samples.exclusiveMatch())
    {
        cout << "   id   ";
    }
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
    if (!_samples.match(samp))
    {
        return false;
    }
    dsm_time_t tt = samp->getTimeTag();
    static dsm_time_t prev_tt = 0;
    dsm_sample_id_t sampid = samp->getId();

    // Format the line leader into a separate string before handling the
    // chosen output format, in case the output format is naked.
    ostringstream leader;

    leader << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%4f") << ' ';

    leader << setprecision(4) << setfill(' ');
    if (prev_tt != 0) {
        double tdiff = (tt - prev_tt) / (double)(USECS_PER_SEC);
        leader << setw(7) << tdiff << ' ';
        if ((warntime < 0 && tdiff < warntime) ||
            (warntime > 0 && tdiff > warntime))
        {
            cerr << "Warning: Sample time skips "
                 << tdiff << " seconds." << endl;
        }
    }
    else
    {
        leader << setw(7) << 0 << ' ';
    }

    if (!_samples.exclusiveMatch())
    {
        leader << setw(2) << setfill(' ') << GET_DSM_ID(sampid) << ',';
        NidasApp* app = NidasApp::getApplicationInstance();
        app->formatSampleId(leader, sampid);
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

    int usage(const char* argv0);

    static int main(int argc, char** argv);

private:

    static const int DEFAULT_PORT = 30000;

    string xmlFileName;

    DumpClient::format_t format;

    float warntime;

    NidasApp app;
    NidasAppArg WarnTime;
};


DataDump::DataDump():
    xmlFileName(),
    format(DumpClient::DEFAULT),
    warntime(0.0),
    app("data_dump"),
    WarnTime("-w,--warntime", "<seconds>",
             "Warn when sample time succeeds the previous more than <seconds>.\n"
             "If <seconds> is negative, then warn when the succeeding time skips\n"
             "backwards.\n", "0")
{
    app.setApplicationInstance();
    app.setupSignals();
}


int DataDump::parseRunstring(int argc, char** argv)
{
    app.enableArguments(app.XmlHeaderFile | app.loggingArgs() |
                        app.FormatHexId | app.FormatSampleId |
                        app.SampleRanges | app.StartTime | app.EndTime |
                        app.Version | app.InputFiles | app.ProcessData |
                        app.Help | WarnTime);

    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    app.InputFiles.setDefaultInput("sock:localhost", DEFAULT_PORT);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        usage(argv[0]);
    }
    warntime = WarnTime.asFloat();

    NidasAppArgv left(args);
    int opt_char;     /* option character */

    while ((opt_char = getopt(left.argc, left.argv, "A7FHnILSU")) != -1) {
	switch (opt_char) {
	case 'A':
	    format = DumpClient::ASCII;
	    break;
	case '7':
	    format = DumpClient::ASCII_7;
	    break;
	case 'F':
	    format = DumpClient::FLOAT;
	    break;
	case 'H':
	    format = DumpClient::HEX_FMT;
	    break;
        case 'n':
            format = DumpClient::NAKED;
            break;
	case 'I':
	    format = DumpClient::IRIG;
	    break;
	case 'L':
	    format = DumpClient::INT32;
	    break;
	case 'S':
	    format = DumpClient::SIGNED_SHORT;
	    break;
	case 'U':
	    format = DumpClient::UNSIGNED_SHORT;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    vector<string> inputs(args.begin()+optind, args.end());
    app.parseInputs(inputs);

    if (app.sampleMatcher().numRanges() == 0)
    {
        throw NidasAppException("At least one sample ID must be specified.");
    }
    return 0;
}

int DataDump::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0
         << " [options] [-A | -7 | -F | -H | -n | -I | -L | -S]"
         << "[inputURL ...]\n"
         << "\
Standard options:\n"
         << app.usage()
         << "data_dump options:\n\
\
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
    -S: ASCII output of signed 16 bit integers (useful for samples from an A2D)\n\
\
    If a format is specified, that format is used for all the samples, except\n\
    that a floating point format is always used for floating point samples.\n\
    Otherwise the format is chosen according to the type in the sample, so\n\
    it is possible to dump samples in different formats.  This is useful for\n\
    dumping both raw and processed samples.  (See example below.)\n\
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
    DataDump dump;

    int res;

    try {
        if ((res = dump.parseRunstring(argc,argv))) return res;

        return dump.run();
    }
    catch (const NidasAppException& ex)
    {
        std::cerr << ex.what() << std::endl;
        std::cerr << "Use -h option to get usage information." << std::endl;
        return 1;
    }
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int DataDump::run() throw()
{
    try {
        AutoProject project;

	IOChannel* iochan = 0;

	if (app.dataFileNames().size() > 0) {
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
	}
	else {
            // We know a default socket address was provided, so it's safe
            // to dereference it.
	    n_u::Socket* sock = new n_u::Socket(*app.socketAddress());
            iochan = new nidas::core::Socket(sock);
	}

        // If you want to process data, get the raw stream
	SampleInputStream sis(iochan, app.processData());
	// SampleStream now owns the iochan ptr.
        sis.setMaxSampleLength(32768);
	// sis.init();
	sis.readInputHeader();
	const SampleInputHeader& header = sis.getInputHeader();

	list<DSMSensor*> allsensors;

        xmlFileName = app.xmlHeaderFile();
	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(),&statbuf) == 0 || app.processData())
        {
            n_u::auto_ptr<xercesc::DOMDocument>
                doc(parseXMLConfigFile(xmlFileName));

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
	if (app.processData()) {
	    list<DSMSensor*>::const_iterator si;
	    for (si = allsensors.begin(); si != allsensors.end(); ++si) {
		DSMSensor* sensor = *si;
		sensor->init();
                //  1. inform the SampleInputStream of what SampleTags to expect
                sis.addSampleTag(sensor->getRawSampleTag());
	    }
	}

        DumpClient dumper(app.sampleMatcher(), format, cout);
        dumper.setWarningTime(warntime);

	if (app.processData()) {
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
                if (app.interrupted()) break;
            }
        }
        catch (n_u::EOFException& e) {
            cerr << e.what() << endl;
        }
        catch (n_u::IOException& e) {
            if (app.processData())
                pipeline.getProcessedSampleSource()->removeSampleClient(&dumper);
            else
                pipeline.getRawSampleSource()->removeSampleClient(&dumper);

            pipeline.disconnect(&sis);
            pipeline.interrupt();
            pipeline.join();
            sis.close();
            throw(e);
        }
	if (app.processData()) {
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
