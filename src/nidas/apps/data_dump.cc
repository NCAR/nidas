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
#include <nidas/core/BadSampleFilter.h>
#include <nidas/core/Variable.h>

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
    typedef enum format
    {
        DEFAULT,
        ASCII,
        HEX_FMT,
        SIGNED_SHORT,
        UNSIGNED_SHORT,
        FLOAT,
        IRIG,
        INT32,
        ASCII_7,
        NAKED
    } format_t;

    DumpClient(const SampleMatcher&, format_t, ostream&);

    virtual ~DumpClient() {}

    void
    flush() throw()
    {
    }

    bool receive(const Sample* samp) throw();

    void printHeader();

    DumpClient::format_t typeToFormat(sampleType t);

    void
    setWarningTime(float w)
    {
        warntime = w;
    }

    void
    setShowDeltaT(bool show)
    {
        showdeltat = show;
    }

    void
    setShowLen(bool show)
    {
        showlen = show;
    }

    void
    setTimeFormat(const std::string& fmt)
    {
        timeformat = fmt;
    }

    void
    setCSV(bool enable)
    {
        csv = enable;
    }

    void setSensors(list<DSMSensor*>& sensors);

private:
    SampleMatcher _samples;

    format_t format;

    ostream& ostr;

    const n_u::EndianConverter* fromLittle;

    dsm_time_t prev_tt;
 
    float warntime;
    bool showdeltat;
    bool showlen;
    string timeformat;
    bool csv;

    vector<string> vnames;
    /// Map a column name to a width.
    map<string, int> widths;

    int
    getWidth(const std::string& name)
    {
        int width = 10;
        map<string, int>::iterator it = widths.find(name);
        if (it != widths.end())
            width = it->second;
        return width;
    }

    std::ostream& setfield(std::ostream& out, const std::string& name,
                           int width = 0);

    void dumpNaked(const Sample* samp);

    DumpClient(const DumpClient&);
    DumpClient& operator=(const DumpClient&);
};

#define DEFTIMEFMT "%Y %m %d %H:%M:%S.%4f"

DumpClient::DumpClient(const SampleMatcher& matcher, format_t fmt,
                       ostream& outstr):
    _samples(matcher),
    format(fmt),
    ostr(outstr),
    fromLittle(n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN)),
    prev_tt(0),
    warntime(0.0),
    showdeltat(true),
    showlen(true),
    timeformat(DEFTIMEFMT),
    csv(false),
    vnames(),
    widths()
{
}

void
DumpClient::setSensors(list<DSMSensor*>& sensors)
{
    // If there is exactly one sample being matched, and there is a sensor
    // sample tag which matches it, then use the sample tag variable names
    // for the header.
    if (!_samples.exclusiveMatch())
        return;
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end() && vnames.empty(); ++si)
    {
        DSMSensor* sensor = *si;

        SampleTagIterator itag = sensor->getSampleTagIterator();
        while (itag.hasNext() && vnames.empty())
        {
            const SampleTag* tag = itag.next();
            if (_samples.match(tag->getId()))
            {
                VariableIterator ivar = tag->getVariableIterator();
                while (ivar.hasNext())
                {
                    const Variable* var = ivar.next();
                    vnames.push_back(var->getPrefix());
                }
            }
        }
    }
}

/**
 * Setup the ostream for the given field, depending on the field
 * separator and field width.
 **/
std::ostream&
DumpClient::setfield(std::ostream& out, const std::string& name, int width)
{
    if (width == 0)
        width = getWidth(name);
    if (name != "datetime")
    {
        out << (csv ? "," : " ");
    }
    if (!csv)
    {
        out << setw(width);
    }
    return out;
}

void
DumpClient::printHeader()
{
    // Setup the widths now.  Columns have set widths and are separated by a
    // single space.
    string datetimehdr = "|--- date time --------|";
    widths["datetime"] = datetimehdr.size();
    widths["len"] = 7;
    widths["deltaT"] = 7;
    // DSM ID width is always 3.
    widths["dsm"] = 3;

    setfield(ostr, "datetime") << (csv ? "datetime" : datetimehdr);
    if (showdeltat)
        setfield(ostr, "deltaT") << "deltaT";
    // IDs width 8
    if (!_samples.exclusiveMatch())
    {
        setfield(ostr, "dsm") << "dsm";
        // Width of the SPS ID is determined by the format.
        NidasApp* app = NidasApp::getApplicationInstance();
        widths["sid"] = app->getIdFormat().decimalWidth();
        // By convention, dsm and sid are always separated by a comma.  And
        // sid always takes up the format width, even in csv.
        ostr << "," << setw(getWidth("sid")) << "sid";
    }
    // len column is width 7, preceded by a space
    if (showlen)
    {
        setfield(ostr, "len") << "len";
    }
    if (!vnames.empty())
    {
        // We know the variable names for all the columns, so use them.
        vector<string>::iterator it;
        for (it = vnames.begin(); it != vnames.end(); ++it)
        {
            setfield(ostr, "data") << *it;
        }
    }
    else
    {
        // Left justify data header when columns unknown.
        setfield(ostr, "data", 1) << "data...";
    }
    ostr << endl;
}

/*
 * This function is not as useful as it seems. Currently in NIDAS,
 * all raw samples from sensors are of type CHAR_ST, and processed samples
 * are FLOAT_ST. So this function does not automagically result in raw
 * data being displayed in its natural format.
 */
DumpClient::format_t
DumpClient::typeToFormat(sampleType t)
{
    static std::map<sampleType, DumpClient::format_t> themap;
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

void
DumpClient::dumpNaked(const Sample* samp)
{
    // Write the raw sample unadorned and unformatted.
    // NIDAS adds a NULL char, '\0', if the user has specified
    // a separator that ends in \r or \n. In this way records are easily
    // scanned with sscanf without adding a NULL. We don't know
    // what the separator actually is, but it should be mostly
    // right to check for a ending "\n\0" or "\r\0" here, and if found,
    // remove the \0.
    size_t n = samp->getDataByteLength();
    const char* ptr = (const char*)samp->getConstVoidDataPtr();
    if (n > 1 && ptr[n - 1] == '\0' &&
        (ptr[n - 2] == '\r' || ptr[n - 2] == '\n'))
        n--;
    ostr.write(ptr, n);
}

bool
DumpClient::receive(const Sample* samp) throw()
{
    if (!_samples.match(samp))
    {
        return false;
    }
    // Naked format trumps everything.
    if (format == NAKED)
    {
        dumpNaked(samp);
        return true;
    }

    dsm_time_t tt = samp->getTimeTag();
    dsm_sample_id_t sampid = samp->getId();

    double tdiff = 0.0;
    if (prev_tt != 0)
    {
        tdiff = (tt - prev_tt) / (double)(USECS_PER_SEC);
        if (warntime > 0 && std::abs(tdiff) >= warntime)
        {
            cerr << "Warning: Sample time skips " << tdiff << " seconds."
                 << endl;
        }
    }
    setfield(ostr, "datetime") << n_u::UTime(tt).format(true, timeformat);

    ostr << setprecision(4);
    if (showdeltat)
    {
        setfield(ostr, "deltaT") << tdiff;
    }

    if (!_samples.exclusiveMatch())
    {
        NidasApp* app = NidasApp::getApplicationInstance();
        setfield(ostr, "dsm") << GET_DSM_ID(sampid);
        // By convention, IDs are always separated by a comma, even when CSV
        // is not in effect.
        ostr << ",";
        app->formatSampleId(ostr, sampid);
    }

    if (showlen)
    {
        setfield(ostr, "len") << samp->getDataByteLength();
    }
    prev_tt = tt;

    // Force floating point samples to be printed in FLOAT format.
    format_t sample_format = format;
    if (samp->getType() == FLOAT_ST)
        sample_format = FLOAT;
    else if (samp->getType() == DOUBLE_ST)
        sample_format = FLOAT;
    else if (format == DEFAULT)
    {
        sample_format = typeToFormat(samp->getType());
    }

    switch (sample_format)
    {
    case ASCII:
    case ASCII_7:
    {
        const char* cp = (const char*)samp->getConstVoidDataPtr();
        size_t l = samp->getDataByteLength();
        if (l > 0 && cp[l - 1] == '\0')
            l--; // exclude trailing '\0'
        // DLOG(("rendering char sample: '")
        //      << n_u::addBackslashSequences(string(cp, l)) << "'");

        char cp7[l];
        for (char* xp = cp7; xp < cp7 + l; ++xp, ++cp)
        {
            *xp = *cp;
            if (sample_format == ASCII_7)
                *xp = *xp & 0x7f;
        }
        setfield(ostr, "data", 1)
            << n_u::addBackslashSequences(string(cp7, l));
    }
    break;
    case HEX_FMT:
    {
        const unsigned char* cp =
            (const unsigned char*)samp->getConstVoidDataPtr();
        ostr << setfill('0');
        for (unsigned int i = 0; i < samp->getDataByteLength(); i++)
            setfield(ostr, "data", 2) << hex << (unsigned int)cp[i];
        ostr << dec << setfill(' ');
    }
    break;
    case SIGNED_SHORT:
    {
        const short* sp = (const short*)samp->getConstVoidDataPtr();
        for (unsigned int i = 0; i < samp->getDataByteLength() / sizeof(short);
             i++)
            setfield(ostr, "data", 6) << sp[i];
    }
    break;
    case UNSIGNED_SHORT:
    {
        const unsigned short* sp =
            (const unsigned short*)samp->getConstVoidDataPtr();
        for (unsigned int i = 0; i < samp->getDataByteLength() / sizeof(short);
             i++)
            setfield(ostr, "data", 6) << sp[i];
    }
    break;
    case FLOAT:
        if (samp->getType() == DOUBLE_ST)
            ostr << setprecision(10);
        else
            ostr << setprecision(5);

        for (unsigned int i = 0; i < samp->getDataLength(); i++)
            setfield(ostr, "data") << samp->getDataValue(i);
        break;
    case IRIG:
    {
        const unsigned char* statusp = IRIGSensor::getStatusPtr(samp);
        unsigned char status = *statusp++;

        dsm_time_t tirig = IRIGSensor::getIRIGTime(samp);
        string tstr = n_u::UTime(tirig).format(true, "%H:%M:%S.%6f");
        ostr << "irig: " << tstr << ", ";

        dsm_time_t tunix = IRIGSensor::getUnixTime(samp);
        if (tunix != 0LL)
        {
            tstr = n_u::UTime(tunix).format(true, "%H:%M:%S.%6f");
            ostr << "unix: " << tstr << ", ";
            ostr << "i-u: " << setfill(' ') << setw(4) << (tirig - tunix)
                 << " us, ";

            ostr << "status: " << setw(2) << setfill('0') << hex << (int)status
                 << dec << '(' << IRIGSensor::shortStatusString(status) << ')';
            ostr << ", seq: " << (int)*statusp++;
            ostr << ", synctgls: " << (int)*statusp++;
            ostr << ", clksteps: " << (int)*statusp++;
            ostr << ", maxbacklog: " << (int)*statusp++;
        }
        else
        {
            ostr << "status: " << setw(2) << setfill('0') << hex << (int)status
                 << dec << '(' << IRIGSensor::shortStatusString(status) << ')';
        }
        ostr << setfill(' ');
    }
    break;
    case INT32:
    {
        const int* lp = (const int*)samp->getConstVoidDataPtr();
        for (unsigned int i = 0; i < samp->getDataByteLength() / sizeof(int);
             i++)
            setfield(ostr, "data", 8) << lp[i];
    }
    break;
    case NAKED:
        break;
    case DEFAULT:
        break;
    }
    ostr << endl;
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
    NidasAppArg NoDeltaT;
    NidasAppArg NoLen;
    NidasAppArg FormatTimeISO;
    NidasAppArg CSV;
    BadSampleFilterArg FilterArg;
};

#define ISOFORMAT "%Y-%m-%dT%H:%M:%S.%4f"

DataDump::DataDump():
    xmlFileName(),
    format(DumpClient::DEFAULT),
    warntime(0.0),
    app("data_dump"),
    WarnTime(
        "-w,--warntime", "<seconds>",
        "Warn when successive sample times differ more than <seconds>.\n",
        "0"),
    NoDeltaT("--nodeltat", "",
             "Do not include the time delta between samples in the output."),
    NoLen("--nolen", "", "Do not include the sample length in the output."),
    FormatTimeISO(
        "--iso", "",
        "Print timestamps without spaces in format: " ISOFORMAT "\n"
        "Times are always printed in UTC, default format: " DEFTIMEFMT),
    CSV("--csv", "",
        "Output data lines as comma-separated values.\n"
        "If only a single sample is selected, the variable names will\n"
        "listed in the header line."),
    FilterArg()
{
    app.setApplicationInstance();
    app.setupSignals();
}

int
DataDump::parseRunstring(int argc, char** argv)
{
    app.enableArguments(app.XmlHeaderFile | app.loggingArgs() |
                        app.FormatHexId | app.FormatSampleId |
                        app.SampleRanges | app.StartTime | app.EndTime |
                        app.Version | app.InputFiles | app.ProcessData |
                        app.Help | app.Version | WarnTime | NoDeltaT | NoLen |
                        FormatTimeISO | CSV | FilterArg);

    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    app.InputFiles.setDefaultInput("sock:localhost", DEFAULT_PORT);
    // Use width 4 for decimal sample id format.
    app.setIdFormat(NidasApp::IdFormat().setDecimalWidth(4));
    app.allowUnrecognized(true);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    warntime = std::abs(WarnTime.asFloat());

    NidasAppArgv left(argv[0], args);
    int opt_char; /* option character */

    while ((opt_char = getopt(left.argc, left.argv, "A7FHnILSU")) != -1)
    {
        switch (opt_char)
        {
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
            std::cerr << "Use -h to see usage info.\n";
            return 1;
        }
    }
    app.parseInputs(left.unparsedArgs(optind));

    if (app.sampleMatcher().numRanges() == 0)
    {
        throw NidasAppException("At least one sample ID must be specified.");
    }
    return 0;
}

int
DataDump::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0
         << " [options] [-A | -7 | -F | -H | -n | -I | -L | -S]"
         << "[inputURL ...]\n"
         << "\
Standard options:\n"
         << app.usage() << "data_dump options:\n\
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
  " << argv0
         << " -i 1,100 -I\n\
Display ASCII data of sensor 200, dsm 1 from sock:localhost:\n\
  " << argv0
         << " -i 1,200 -A\n\
Display ASCII data from archive files:\n\
  " << argv0
         << " -i 1,200 -A xxx.dat yyy.dat\n\
Hex dump of sensor ids 200 through 210 using configuration in ads3.xml:\n\
  " << argv0
         << " -i 3,200-210 -H -x ads3.xml xxx.dat\n\
Display processed data of sample 1 of sensor 200:\n\
  " << argv0
         << " -i 3,201 -p sock:hyper\n\
Display processed data of sample 1, sensor 200, from unix socket:\n\
  " << argv0
         << " -i 3,201 -p unix:/tmp/dsm\n\
Display all raw and processed samples in their default format:\n\
  " << argv0
         << " -i -1,-1 -p -x path/to/project.xml file.dat\n"
         << endl;
    return 1;
}

/* static */
int
DataDump::main(int argc, char** argv)
{
    DataDump dump;

    int res;

    try
    {
        if ((res = dump.parseRunstring(argc, argv)))
            return res;

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

int
DataDump::run() throw()
{
    try
    {
        AutoProject project;

        IOChannel* iochan = 0;

        if (app.dataFileNames().size() > 0)
        {
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
        }
        else
        {
            // We know a default socket address was provided, so it's safe
            // to dereference it.
            n_u::Socket* sock = new n_u::Socket(*app.socketAddress());
            iochan = new nidas::core::Socket(sock);
        }

        // If you want to process data, get the raw stream
        SampleInputStream sis(iochan, app.processData());
        // SampleStream now owns the iochan ptr.

        BadSampleFilter& bsf = FilterArg.getFilter();
        bsf.setDefaultTimeRange(app.getStartTime(), app.getEndTime());
        sis.setBadSampleFilter(bsf);
        sis.readInputHeader();
        const SampleInputHeader& header = sis.getInputHeader();

        list<DSMSensor*> allsensors;

        xmlFileName = app.xmlHeaderFile();
        if (xmlFileName.length() == 0)
            xmlFileName = header.getConfigName();
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

        struct stat statbuf;
        if (::stat(xmlFileName.c_str(), &statbuf) == 0 || app.processData())
        {
            n_u::auto_ptr<xercesc::DOMDocument> doc(
                parseXMLConfigFile(xmlFileName));

            Project::getInstance()->fromDOMElement(doc->getDocumentElement());

            DSMConfigIterator di =
                Project::getInstance()->getDSMConfigIterator();

            for (; di.hasNext();)
            {
                const DSMConfig* dsm = di.next();
                const list<DSMSensor*>& sensors = dsm->getSensors();
                allsensors.insert(allsensors.end(), sensors.begin(),
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
        if (app.processData())
        {
            list<DSMSensor*>::const_iterator si;
            for (si = allsensors.begin(); si != allsensors.end(); ++si)
            {
                DSMSensor* sensor = *si;
                sensor->init();
                //  1. inform the SampleInputStream of what SampleTags to
                //  expect
                sis.addSampleTag(sensor->getRawSampleTag());
            }
        }

        DumpClient dumper(app.sampleMatcher(), format, cout);
        dumper.setWarningTime(warntime);
        dumper.setShowDeltaT(!NoDeltaT.asBool());
        dumper.setShowLen(!NoLen.asBool());
        dumper.setSensors(allsensors);
        dumper.setCSV(CSV.asBool());

        if (FormatTimeISO.asBool())
            dumper.setTimeFormat(ISOFORMAT);

        if (app.processData())
        {
            // 2. connect the pipeline to the SampleInputStream.
            pipeline.connect(&sis);
            pipeline.getProcessedSampleSource()->addSampleClient(&dumper);
            // 3. connect the client to the pipeline
            pipeline.getRawSampleSource()->addSampleClient(&dumper);
        }
        else
        {
            sis.addSampleClient(&dumper);
        }

        if (format != DumpClient::NAKED)
            dumper.printHeader();

        try
        {
            for (;;)
            {
                sis.readSamples();
                if (app.interrupted())
                    break;
            }
        }
        catch (n_u::EOFException& e)
        {
            cerr << e.what() << endl;
        }
        catch (n_u::IOException& e)
        {
            if (app.processData())
                pipeline.getProcessedSampleSource()->removeSampleClient(
                    &dumper);
            else
                pipeline.getRawSampleSource()->removeSampleClient(&dumper);

            pipeline.disconnect(&sis);
            pipeline.interrupt();
            pipeline.join();
            sis.close();
            throw(e);
        }
        if (app.processData())
        {
            pipeline.disconnect(&sis);
            pipeline.flush();
            pipeline.getProcessedSampleSource()->removeSampleClient(&dumper);
            pipeline.getRawSampleSource()->removeSampleClient(&dumper);
        }
        else
        {
            sis.removeSampleClient(&dumper);
        }
        sis.close();
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception& e)
    {
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
        return 1;
    }
    return 0;
}

int
main(int argc, char** argv)
{
    return DataDump::main(argc, argv);
}
