/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include <nidas/dynld/RawSampleInputStream.h>

#include <nidas/dynld/isff/NetcdfRPCOutput.h>
#include <nidas/dynld/isff/NetcdfRPCChannel.h>

#include <nidas/core/FileSet.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/NearestResampler.h>
#include <nidas/core/NearestResamplerAtRate.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/XMLParser.h>

#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Version.h>
#include <nidas/core/Socket.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Process.h>
#include <nidas/util/auto_ptr.h>
#include <nidas/core/BadSampleFilter.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <memory>
#include <unistd.h>

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

    void flush() throw()
    {
        _ostr.flush();
    }

    bool receive(const Sample* samp) throw();

    void printHeader(vector<const Variable*> vars);

    void setStartTime(const n_u::UTime& val)
    {
        _startTime = val;
        _checkStart = true;
    }

    void setEndTime(const n_u::UTime& val)
    {
        _endTime = val;
        _checkEnd = true;
    }

    void setDOS(bool val)
    {
        _dosOut = val;
    }

    bool getDOS() const
    {
        return _dosOut;
    }

    bool finished() const
    {
        return _finished;
    }

private:

    format_t _format;

    ostream& _ostr;

    n_u::UTime _startTime;

    n_u::UTime _endTime;

    bool _checkStart;

    bool _checkEnd;

    bool _dosOut;

    int _asciiPrecision;

    bool _finished;
};


class DataPrep
{
public:

    DataPrep();

    ~DataPrep();

    int parseRunstring(int argc, char** argv);

    void parseNcServerSpec(const std::string& spec);

    int run() throw();

    static int main(int argc, char** argv);

    int usage();

    map<double, vector<const Variable*> >
    matchVariables(const Project&, set<const DSMConfig*>& activeDsms,
                   set<DSMSensor*>& activeSensors);

    // default initialization values, which are displayed in usage() method.
    static const int defaultNCInterval = 1;
    static const int defaultNCLength = 86400;
    static const float defaultNCFillValue;
    static const int defaultNCTimeout = 60;
    static const int defaultNCBatchPeriod = 300;

private:

    NidasApp _app;

    string _xmlFileName;

    list<string> _dataFileNames;

    static const int DEFAULT_PORT = 30000;

    float _sorterLength;

    DumpClient::format_t _format;

    map<double, vector<Variable*> > _reqVarsByRate;

    map<Variable*, string> _sites;

    n_u::UTime _startTime;

    n_u::UTime _endTime;

    std::string _configName;

    bool _middleTimeTags;

    bool _dosOut;

    bool _doHeader;

    bool _clipping;

    static const char* _isffDatasetsXML;

    static const char* _isfsDatasetsXML;

    int _asciiPrecision;

    string _ncserver;

    string _ncdir;

    string _ncfile;

    int _ncinterval;

    int _nclength;

    string _nccdl;

    float _ncfill;

    int _nctimeout;

    int _ncbatchperiod;

    list<Resampler*> _resamplers;

    string _dsmName;

    string _datasetName;

    BadSampleFilterArg _FilterArg;
    NidasAppArg DataVariables;
    NidasAppArg DataRate;
    NidasAppArg DatasetName;
    NidasAppArg ConfigsName;
    NidasAppArg DSMName;
    NidasAppArg DumpASCII;
    NidasAppArg DumpBINARY;
    NidasAppArg DOSOutput;
    NidasAppArg Clipping;
    NidasAppArg SorterLength;
    NidasAppArg Precision;
    NidasAppArg NoHeader;
    NidasAppArg NetcdfOutput;
    NidasAppArg HeapSize;
};

const float DataPrep::defaultNCFillValue = 1.e37;

DataPrep::DataPrep(): 
    _app("prep"),_xmlFileName(),_dataFileNames(),
    _sorterLength(1.00), _format(DumpClient::ASCII),
    _reqVarsByRate(),_sites(),
    _startTime((time_t)0),_endTime((time_t)0),_configName(),
    _middleTimeTags(true),_dosOut(false),_doHeader(true),
    _clipping(false),
    _asciiPrecision(5),
    _ncserver(),_ncdir(),_ncfile(),
    _ncinterval(defaultNCInterval),_nclength(defaultNCLength),
    _nccdl(), _ncfill(defaultNCFillValue),_nctimeout(defaultNCTimeout),
    _ncbatchperiod(defaultNCBatchPeriod),
    _resamplers(),_dsmName(),_datasetName(),
    _FilterArg(),
    DataVariables
    ("-D", "var[,var,...]",
     "One or more variable names to output at the current rate"),
    DataRate
    ("-r,-R", "<rate in Hz>",
     "Set the resample rate, in Hz, for all successive variables "
     "specified with -D.\n"
     "With -r, output timetags will be in middle of periods.\n"
     "With -R, output timetags will be at integral deltaTs.\n"
     "When writing NetCDF files, it can be useful for prep to generate\n"
     "output at several rates:  -r 1 -D v1,v2 -r 20 -D v3,v4\n"
     "If multiple -D options, specify the rate BEFORE the -D var."),
    DatasetName("-S,--dataset", "<datasetname>",
                "dataset name from $ISFS/projects/$PROJECT/"
                "ISFS/config/datasets.xml"),
    ConfigsName("-c,--config", "<configname>",
                "(optional) name of configuration period to use, "
                "from configs.xml"),
    DSMName("-d,--dsm", "<dsm>",
            "Look for a <fileset> belonging to the given dsm to "
            "determine input file names."),
    DumpASCII("-A", "", "ascii output (default)"),
    DumpBINARY("-C", "",
               "binary column output, double seconds since "
               "Jan 1, 1970, \nfollowed by floats for each var"),
    DOSOutput("-w,--dos", "", "windows/dos output "
              "(records terminated by CRNL instead of just NL)"),
    Clipping
    ("--clip", "",
     "Clip the output samples to the given time range,\n"
     "and expand the input time boundaries by 5 minutes.\n"
     "The input times are expanded to catch all raw samples\n"
     "whose processed sample times might fall within the output times.\n"
     "This option only applies to netcdf outputs.\n"),
    SorterLength("-s,--sortlen", "<seconds>",
                 "sorter length for processed samples in "
                 "floating point seconds (optional)", "1.0"),
    Precision("-p,--precision", "ndigits",
              "number of digits in ASCII output values", "5"),
    NoHeader("-H,--noheader", "",
             "do not print initial 2-line ASCII header of "
             "variable names and units"),
    NetcdfOutput
    ("-n,--netcdf",
     "server:dir:file:interval:length:cdlfile:missing:timeout:batchperiod"),
    HeapSize("--heapsize", "<kilobytes>",
             "Set the sizes of the raw and processed sorter heaps in "
             "kilobytes.", "1000")
{
}

DataPrep::~DataPrep()
{
    map<double, vector<Variable*> >::const_iterator mi = _reqVarsByRate.begin();
    for ( ; mi != _reqVarsByRate.end(); ++mi) {
        const vector<Variable*>& reqVars = mi->second;
        vector<Variable*>::const_iterator rvi = reqVars.begin();
        for ( ; rvi != reqVars.end(); ++rvi) {
            Variable* v = *rvi;
            delete v;
        }
    }
    list<Resampler*>::iterator ri;
    for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri)
    {
        delete (*ri);
    }
}


DumpClient::DumpClient(format_t fmt,ostream &outstr,int precision):
    _format(fmt),_ostr(outstr),_startTime((time_t)0),_endTime((time_t)0),
    _checkStart(false),_checkEnd(false),_dosOut(false),
    _asciiPrecision(precision),_finished(false)
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
    if (_dosOut) cout << '\r';
    cout << endl;
    vi = vars.begin();
    for (; vi != vars.end(); ++vi) {
        const Variable* var = *vi;
        if (var->getConverter())
            cout << '"' << var->getConverter()->getUnits() << "\" ";
        else
            cout << '"' << var->getUnits() << "\" ";
    }
    if (_dosOut) cout << '\r';
    cout << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    if (_checkStart && tt < _startTime.toUsecs()) return false;
    if (_checkEnd && tt > _endTime.toUsecs()) {
        _finished = true;
        return false;
    }

    VLOG(("") << "sampid=" << GET_DSM_ID(samp->getId()) << ','
         << GET_SHORT_ID(samp->getId()));

    switch(_format) {
    case ASCII:
        {
            n_u::UTime ut(tt);
            _ostr << ut.format(true,"%Y %m %d %H:%M:%S.%4f");

            _ostr << setprecision(_asciiPrecision) << setfill(' ');
            for (unsigned int i = 0; i < samp->getDataLength(); i++)
                _ostr << ' ' << setw(10) << samp->getDataValue(i);
            if (_dosOut) cout << '\r';
            _ostr << endl;
        }
        break;
    case BINARY1:
        {

            int fsecs = tt % USECS_PER_SEC;
            double ut = (double)((tt - fsecs) / USECS_PER_SEC) +
                (double) fsecs / USECS_PER_SEC;

            _ostr.write((const char*)&ut,sizeof(ut));
            const float* fp =
                (const float*) samp->getConstVoidDataPtr();
            for (unsigned int i = 0;
                 i < samp->getDataByteLength()/sizeof(float); i++)
                _ostr.write((const char*)(fp+i),sizeof(float));
        }
        break;
    case BINARY2:
        {

            _ostr.write((const char*)&tt,sizeof(tt));
            const float* fp =
                (const float*) samp->getConstVoidDataPtr();
            for (unsigned int i = 0;
                 i < samp->getDataByteLength()/sizeof(float); i++)
                _ostr.write((const char*)(fp+i),sizeof(float));
        }
        break;
    }
    return true;
}

void
DataPrep::
parseNcServerSpec(const std::string& spec)
{
    string ncarg(spec);
    string::size_type i1=0,i2;

    i2 = ncarg.find(':',i1);
    if (i2 > i1) _ncserver = ncarg.substr(i1,i2-i1);
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _ncdir = ncarg.substr(i1,i2-i1);
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _ncfile = ncarg.substr(i1,i2-i1);
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _ncinterval = atoi(ncarg.substr(i1,i2-i1).c_str());
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _nclength = atoi(ncarg.substr(i1,i2-i1).c_str());
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _nccdl = ncarg.substr(i1,i2-i1);
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _ncfill = atof(ncarg.substr(i1,i2-i1).c_str());
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _nctimeout = atoi(ncarg.substr(i1,i2-i1).c_str());
    if (i2 == string::npos) return;

    i1 = i2 + 1;
    i2 = ncarg.find(':',i1);
    if (i2 > i1) _ncbatchperiod = atoi(ncarg.substr(i1,i2-i1).c_str());
    if (i2 == string::npos) return;
}


int DataPrep::parseRunstring(int argc, char** argv)
{
    const char* p1,*p2;
    double rate = 0.0;

    std::ostringstream nchelp;
    nchelp << "server: host name of system running nc_server RPC process\n"
        "dir: directory on server to write files\n"
        "file: format of NetCDF file names. For example: xxx_%Y%m%d.nc\n"
        "interval: deltaT in seconds between time values in file. Default: "
        << defaultNCInterval << "\n"
        "length: length of file, in seconds. 0 for no limit to the file size.\n"
        "        Default: " << defaultNCInterval << "\n"
        "cdlfile: name of NetCDF CDL file on server that is used for\n"
        "         initialization of new files\n"
        "missing: missing data value in file. Default: "
        << setprecision(2) << defaultNCFillValue << "\n"
        "timeout: time in seconds that nc_server is expected to respond.\n"
        "         Default: " << defaultNCTimeout << "\n"
        "batchperiod: check for response back from server after this number\n"
        "             of seconds. Default: " << defaultNCInterval << "\n";
    NetcdfOutput.setUsageString(nchelp.str());

    // prep uses -B and -E for window times, but keep the long flags
    _app.StartTime.setFlags("-B,--start");
    _app.EndTime.setFlags("-E,--end");

    _app.enableArguments(_app.InputFiles | DataVariables | DataRate |
                         _app.StartTime | _app.EndTime |
                         _app.InputFiles |
                         DatasetName | ConfigsName | DSMName |
                         DumpASCII | DumpBINARY | DOSOutput |
                         NetcdfOutput | Clipping | _FilterArg |
                         SorterLength | HeapSize | Precision | NoHeader |
                         _app.loggingArgs() | _app.XmlHeaderFile |
                         _app.Version | _app.Help);

    _app.InputFiles.allowFiles = true;
    _app.InputFiles.allowSockets = true;
    _app.InputFiles.setDefaultInput("", DEFAULT_PORT);

    _app.startArgs(argc, argv);
    NidasAppArg* arg;
    while ((arg = _app.parseNext()))
    {
        if (_app.helpRequested())
        {
            return usage();
        }
        if (arg == &DumpASCII)
            _format = DumpClient::ASCII;
        else if (arg == &DumpBINARY)
            _format = DumpClient::BINARY1;
        else if (arg == &DataRate)
        {
            rate = arg->asFloat();
            if (rate < 0)
            {
                cerr << "Invalid resample rate: " << arg->getValue() << endl;
                return 1;
            }
            _middleTimeTags = arg->getFlag() == "-r";
        }
        else if (arg == &DataVariables)
        {
            string opt = arg->getValue();
            p1 = opt.c_str();
            while ((p2 = strchr(p1,','))) {
                Variable *var = new Variable();
                const char* ph = strchr(p1,'#');
                if (ph && ph < p2) {
                    var->setName(string(p1,ph-p1));
                    ph++;
                    _sites[var] = string(ph,p2-ph);
                } else {
                    var->setName(string(p1,p2-p1));
                    _sites[var] = "";
                }
                _reqVarsByRate[rate].push_back(var);
                p1 = p2 + 1;
            }

            Variable *var = new Variable();
            const char* ph = strchr(p1,'#');
            if (ph) {
                var->setName(string(p1,ph-p1));
                ph++;
                _sites[var] = string(ph);
            }
            else {
                var->setName(string(p1));
                _sites[var] = "";
            }
            _reqVarsByRate[rate].push_back(var);
        }
        else if (arg == &NetcdfOutput)
        {
            parseNcServerSpec(arg->getValue());
        }
    }
    _app.parseInputs(_app.unparsedArgs());

    // NidasApp uses MIN and MAX as defaults for unset times, but DataPrep
    // expects zero.
    if (_app.getStartTime() != LONG_LONG_MIN)
        _startTime = _app.getStartTime();
    if (_app.getEndTime() != LONG_LONG_MAX)
        _endTime = _app.getEndTime();
    _datasetName = DatasetName.getValue();
    _configName = ConfigsName.getValue();
    _dsmName = DSMName.getValue();
    _dosOut = DOSOutput.asBool();
    _doHeader = !NoHeader.asBool();
    _xmlFileName = _app.xmlHeaderFile();
    _clipping = Clipping.asBool();
    _sorterLength = SorterLength.asFloat();
    if (_sorterLength < 0 || _sorterLength > 10000)
    {
        cerr << "Invalid sorter length: " << SorterLength.getValue() << endl;
        return 1;
    }

    _asciiPrecision = Precision.asInt();
    if (_asciiPrecision < 1)
    {
        cerr << "Invalid precision: " << Precision.getValue() << endl;
        return 1;
    }

    if (_reqVarsByRate.empty()) {
        cerr << "no variables requested, must have one or more -D options"
             << endl;
        return 1;
    }

    /* If one set of variables was requested, apply the rate to those variables.
     * Otherwise the rate applies to a following set of variables.
     */
    if (_reqVarsByRate.size() == 1 && rate > 0.0) {
        map<double, vector<Variable*> >::iterator mi = _reqVarsByRate.find(0.0);
        if (mi != _reqVarsByRate.end()) {
            _reqVarsByRate[rate] = _reqVarsByRate[0.0];
            _reqVarsByRate.erase(mi);
        }
    }

    _dataFileNames = _app.dataFileNames();

    // must specify either:
    //  1. some data files to read, and optional begin and end times,
    //  2. a socket to connect to
    //  3. or a time period and a $PROJECT environment variable
    if (_dataFileNames.size() == 0 && !_app.socketAddress() &&
        _startTime.toUsecs() == 0)
    {
        cerr << "No inputs and no -B begin time." << endl;
        return 1;
    }
    if (_startTime.toUsecs() != 0 && _endTime.toUsecs() == 0)
    {
        _endTime = _startTime + 90 * USECS_PER_DAY;
    }
    return 0;
}

int DataPrep::usage()
{
    string argv0 = _app.getProcessName();
    cerr << "\
Usage: " << argv0 << " [options] [input ...]\n\
    If the XML file name is not specified with -x, then it is \n\
       determined by either reading the data file header or from\n\
       $ISFS/projects/$PROJECT/ISFS/config/configs.xml\n\
    input: data input (optional). One of the following:\n\
        sock:host[:port]          Default port is " << DEFAULT_PORT << "\n\
        unix:sockpath             unix socket name\n\
        file [file ...]           one or more archive file names\n\
\n\
If no inputs are specified, then the -B time option must be given, and\n" <<
argv0 << " will read $ISFS/projects/$PROJECT/ISFS/config/configs.xml, to\n\
find an xml configuration for the begin time, read it to find a\n\
<fileset> archive for the given variables, and then open data files\n\
matching the <fileset> path descriptor and time period.\n\
\n" <<
argv0 << " does simple resampling, using the nearest sample to the times of the first\n\
variable requested, or if the -r or -R rate options are specified, \n\
to evenly spaced times at the given rate.\n\
\n\
Examples:\n" <<
	argv0 << " -D u.10m,v.10m,w.10m -B \"2006 jun 10 00:00\" -E \"2006 jul 3 00:00\"\n" <<
	argv0 << " -D u.10m,v.10m,w.10m sock:dsmhost\n" <<
	argv0 << " -D u.10m,v.10m,w.10m -r 60 unix:/tmp/data_socket\n" <<
        "\n\
Notes on choosing rates with -r or -R:\n\
    For rates less than 1 Hz it is best to choose a value \n\
    such that 10^6/rate is an integer.\n\
    If you really want rate=1/3 Hz, specify rate to 7 significant figures,\n\
    0.3333333, and you will avoid round off errors in the time tag. \n\
    Output rates > 1 should be integers, or of a value with enough significant\n\
    figures such that 10^6/rate is an integer."
         << endl;
    cerr <<
"\nOptions:\n"
         << _app.usage();
    return 1;
}

/* static */
int DataPrep::main(int argc, char** argv)
{
    DataPrep dump;
    NidasApp::setupSignals();

    int res;

    if ((res = dump.parseRunstring(argc,argv))) return res;

    return dump.run();
}

map<double, vector<const Variable*> >
DataPrep::matchVariables(const Project& project,
    set<const DSMConfig*>& activeDsms,
    set<DSMSensor*>& activeSensors)
{
    map<double, vector<const Variable*> > variables;

    map<double, vector<Variable*> >::const_iterator mi = _reqVarsByRate.begin();
    for ( ; mi != _reqVarsByRate.end(); ++mi) {
        double rate = mi->first;
        const vector<Variable*>& reqVars = mi->second;
        vector<Variable*>::const_iterator rvi = reqVars.begin();

        for ( ; rvi != reqVars.end(); ++rvi) {
            Variable* reqvar = *rvi;
            bool match = false;

            // Check for match of variable against all dsms.  The site may
            // not be specified in the requested variable, and so a
            // variable name like T.1m may match more than one dsm.
            DSMConfigIterator di = project.getDSMConfigIterator();
            for ( ; di.hasNext(); ) {
                const DSMConfig* dsm = di.next();

                const list<DSMSensor*>& sensors = dsm->getSensors();
                list<DSMSensor*>::const_iterator si = sensors.begin();
                for (; si != sensors.end(); ++si) {
                    DSMSensor* sensor = *si;
                    VariableIterator vi = sensor->getVariableIterator();
                    for ( ; vi.hasNext(); ) {
                        const Variable* var = vi.next();
                        VLOG(("")
                             << "var=" << var->getName() <<
                             ":" << var->getSite()->getName() <<
                             '(' << var->getStation() << "), " <<
                             ", reqvar=" << reqvar->getName() <<
                             ":" << (reqvar->getSite() ?
                                     reqvar->getSite()->getName(): "unk") <<
                             '(' << reqvar->getStation() << "), " <<
                             ", match=" << ((*var == *reqvar)) <<
                             ", closematch=" << var->closeMatch(*reqvar));
                        if (*var == *reqvar || var->closeMatch(*reqvar))
                        {
                            // Add variable once for a match
                            // A variable by a given name for a site
                            // may come from more than one sensor and sample.
                            if (!match)
                                variables[rate].push_back(var);
                            activeSensors.insert(sensor);
                            activeDsms.insert(dsm);
                            match = true;
                        }
                    }
                }
            }
            if (!match) throw
                n_u::InvalidParameterException(_app.getProcessName(),
                                               "cannot find variable",
                                               reqvar->getName());
        }
    }
    return variables;
}


template <typename T>
std::ostream&
operator<<(std::ostream& out, const std::pair<const std::string&, const T&>& x)
{
    out << " " << x.first << "=\"" << x.second << "\"";
    return out;
}


template <typename T>
std::pair<const std::string&, const T&>
make_xatt(const std::string& name, const T& value){
    return std::make_pair(name, value);
}


int DataPrep::run() throw()
{
    try {

        Project project;

        if (_datasetName.length() > 0)
            project.setDataset(_app.getDataset(_datasetName));

        IOChannel* iochan = 0;

        if (_xmlFileName.length() > 0) {

            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);

            n_u::auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));

            project.fromDOMElement(doc->getDocumentElement());
        }

        if (_app.socketAddress()) {
            if (_xmlFileName.length() == 0) {

                string configsXMLName = _app.getConfigsXML();
                ProjectConfigs configs;
                configs.parseXML(configsXMLName);
                ILOG(("parsed:") <<  configsXMLName);
                
                const ProjectConfig* cfg;
                if (_configName.length() > 0)
                    cfg = configs.getConfig(_configName);
                else
                    cfg = configs.getConfig(n_u::UTime());

                cfg->initProject(project);
                // cerr << "cfg=" <<  cfg->getName() << endl;
                _xmlFileName = n_u::Process::expandEnvVars(cfg->getXMLName());
            }
            n_u::Socket* sock = 0;
            for (int i = 0; !sock && !_app.interrupted(); i++) {
                try {
                    sock = new n_u::Socket(*_app.socketAddress());
                }
                catch(const n_u::IOException& e) {
                    if (i > 2)
                        WLOG(("%s: retrying", e.what()));
                    sleep(10);
                }
            }
            iochan = new nidas::core::Socket(sock);
            // iochan = iosock->connect();
            // if (iochan != iosock) {
                // iosock->close();
                // delete iosock;
            // }
        }
        else {
            nidas::core::FileSet* fset;
            if (_dataFileNames.size() == 0) {
                // User has not specified the xml file. Get
                // the ProjectConfig from the configName or startTime
                // using the configs XML file, then parse the
                // XML of the ProjectConfig.
                if (_xmlFileName.length() == 0)
                {
                    string configsXMLName = _app.getConfigsXML();
                    ProjectConfigs configs;
                    configs.parseXML(configsXMLName);
                    ILOG(("parsed:") <<  configsXMLName);
                    const ProjectConfig* cfg = 0;

                    if (_configName.length() > 0)
                        cfg = configs.getConfig(_configName);
                    else
                        cfg = configs.getConfig(_startTime);

                    cfg->initProject(project);
                    if (_startTime.toUsecs() == 0) _startTime = cfg->getBeginTime();
                    if (_endTime.toUsecs() == 0) _endTime = cfg->getEndTime();
                    _xmlFileName = n_u::Process::expandEnvVars(cfg->getXMLName());
                }

                list<nidas::core::FileSet*> fsets;
                if (_dsmName.length() > 0) {
                    fsets = project.findSampleOutputStreamFileSets(_dsmName);
                    if (fsets.empty()) {
                        PLOG(("Cannot find a FileSet for dsm ") << _dsmName);
                        return 1;
                    }
                    if (fsets.size() > 1) {
                        PLOG(("Multple filesets found for dsm ") << _dsmName);
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
                    PLOG(("Multple filesets found for dsms. "
                          "Pass a -d dsmname parameter to select one"));
                    return 1;
                }

                // must clone, since fsets.front() belongs to project
                fset = fsets.front()->clone();

                if (_startTime.toUsecs() != 0)
                {
                    n_u::UTime xtime = _startTime;
                    if (_clipping)
                    {
                        xtime = xtime - 300*USECS_PER_SEC;
                        ILOG(("expanding input start time to ")
                             << xtime.format(true, "%Y %m %d %H:%M:%S"));
                    }
                    fset->setStartTime(xtime);
                }
                if (_endTime.toUsecs() != 0)
                {
                    n_u::UTime xtime = _endTime;
                    if (_clipping)
                    {
                        xtime = xtime + 300*USECS_PER_SEC;
                        ILOG(("expanding input end time to ")
                             << xtime.format(true, "%Y %m %d %H:%M:%S"));
                    }
                    fset->setEndTime(xtime);
                }
            }
            else {
                fset = nidas::core::FileSet::getFileSet(_dataFileNames);
            }
            iochan = fset;
        }

        RawSampleInputStream sis(iochan);
        BadSampleFilter& bsf = _FilterArg.getFilter();
        bsf.setDefaultTimeRange(_startTime, _endTime);
        sis.setBadSampleFilter(bsf);

        SamplePipeline pipeline;
        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(_sorterLength);
        pipeline.setProcSorterLength(_sorterLength);
        pipeline.setRawHeapMax(HeapSize.asInt() * 1024);
        pipeline.setProcHeapMax(HeapSize.asInt() * 1024);
        DLOG(("pipeline heap sizes set to ")
             << HeapSize.asInt()*1024 << " KB");
        pipeline.setRawLateSampleCacheSize(0);
        pipeline.setProcLateSampleCacheSize(5);

        if (_xmlFileName.length() == 0) {
            sis.readInputHeader();
            const SampleInputHeader& header = sis.getInputHeader();
            _xmlFileName = header.getConfigName();
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);

            n_u::auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));

            project.fromDOMElement(doc->getDocumentElement());
        }

        project.setConfigName(_xmlFileName);

        map<double, vector<Variable*> >::const_iterator mi = _reqVarsByRate.begin();
        for ( ; mi != _reqVarsByRate.end(); ++mi) {
            const vector<Variable*>& reqVars = mi->second;

            vector<Variable*>::const_iterator vi = reqVars.begin();
            for ( ; vi != reqVars.end(); ++vi) {
                Variable* var = *vi;
                const string& sitestr = _sites[var];
                if (sitestr.length() > 0) {
                    istringstream strm(sitestr);
                    int istn;
                    strm >> istn;
                    Site* site;
                    if (strm.fail())
                        site = Project::getInstance()->findSite(sitestr);
                    else
                        site = Project::getInstance()->findSite(istn);
                    if (!site) {
                        ostringstream ost;
                        ost << "cannot find site " << sitestr << " for variable " <<
                            var->getName();
                        throw n_u::Exception(ost.str());
                    }
                    var->setSite(site);
                }
                else {
                    // if last field of a requested variable matches a site, set it.
                    // This is a hack, since the last field may not necessarily
                    // be a site name, but...
                    const string& vname = var->getName();
                    size_t dot = vname.rfind('.');
                    if (dot != string::npos) {
                        const string sitestr = vname.substr(dot+1);
                        if (!sitestr.empty()) {
                            VLOG(("looking up suffix ") << sitestr
                                 << " of variable "
                                 << vname << " as a site...");
                            Site* site;
                            site = Project::getInstance()->findSite(sitestr);
                            if (site) {
                                var->setName(vname.substr(0, dot));
                                var->setSite(site);
                                ILOG(("variable ") << var->getName()
                                     << " setting implied site " << sitestr);
                            }
                            else
                            {
                                VLOG(("no site found."));
                            }
                        }

                    }
                }
            }
        }

        // match the variables.
        // on a match:
        //  1. save the var to pass to the resampler
        //  2. save the sensor in a set
        //  3. save the dsm in a set
        // later:
        //  1. init the sensors
        //  2. add the resampler as a processedSampleClient of the sensor

        set<DSMSensor*> activeSensors;
        set<const DSMConfig*> activeDsms;
        typedef map<double, vector<const Variable*> > var_by_rate_t;
        var_by_rate_t variablesByRate =
            matchVariables(project,activeDsms,activeSensors);

        {
            static n_u::LogContext lp(LOG_VERBOSE);
            if (lp.active())
            {
                n_u::LogMessage msg(&lp);
                for (var_by_rate_t::iterator it = variablesByRate.begin();
                     it != variablesByRate.end(); ++it)
                {
                    vector<const Variable*> &variables = it->second;
                    msg << "rate=" << it->first << ":";
                    for (vector<const Variable*>::iterator iv = variables.begin();
                         iv != variables.end(); ++iv)
                    {
                        msg << " " << (*iv)->getName()
                            << "(" << (*iv)->getStation() << ")";
                    }
                    msg << "; ";
                }
            }
        }

        set<DSMSensor*>::const_iterator si = activeSensors.begin();
        for ( ; si != activeSensors.end(); ++si) {
            DSMSensor* sensor = *si;
            sensor->init();
            sis.addSampleTag(sensor->getRawSampleTag());
            SampleTagIterator sti = sensor->getSampleTagIterator();
            for ( ; sti.hasNext(); ) {
                const SampleTag* stag = sti.next();
                pipeline.getProcessedSampleSource()->addSampleTag(stag);
            }
        }

        pipeline.connect(&sis);

        list<Resampler*>::const_iterator ri;

        if (_ncserver.length() == 0) {
            map<double, vector<const Variable*> >::const_iterator mi =
                variablesByRate.begin();
            for ( ; mi != variablesByRate.end(); ++mi) {
                double rate = mi->first;
                const vector<const Variable*>& vars = mi->second;

                if (rate > 0.0) {
                    NearestResamplerAtRate* smplr =
                        new NearestResamplerAtRate(vars,false);
                    smplr->setRate(rate);
                    smplr->setFillGaps(true);
                    smplr->setMiddleTimeTags(_middleTimeTags);
                    _resamplers.push_back(smplr);
                }
                else {
                    _resamplers.push_back(new NearestResampler(vars,false));
                }
            }

            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri)
                (*ri)->connect(pipeline.getProcessedSampleSource());

            DumpClient dumper(_format,cout,_asciiPrecision);
            dumper.setDOS(_dosOut);

            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri)
                (*ri)->addSampleClient(&dumper);

            try {
                if (_startTime.toUsecs() != 0) {
                    DLOG(("searching for time ") <<
                        _startTime.format(true,"%Y %m %d %H:%M:%S"));
                    sis.search(_startTime);
                    DLOG(("search done."));
                    dumper.setStartTime(_startTime);
                }
                if (_endTime.toUsecs() != 0) dumper.setEndTime(_endTime);

                if (_doHeader && variablesByRate.size() == 1)
                    dumper.printHeader(variablesByRate.begin()->second);

                for (;;) {
                    sis.readSamples();
                    if (dumper.finished() || _app.interrupted()) break;
                }
            }
            catch (n_u::EOFException& e) {
                cerr << "EOF received" << endl;
            }
            catch (n_u::IOException& e) {
                for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                    (*ri)->removeSampleClient(&dumper);
                    (*ri)->disconnect(pipeline.getProcessedSampleSource());
                }
                pipeline.disconnect(&sis);
                sis.close();
                throw e;
            }
            pipeline.disconnect(&sis);
            sis.close();
            pipeline.flush();
            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                (*ri)->removeSampleClient(&dumper);
            }
        }
        else {
            map<Resampler*, double> ratesByResampler;
            map<double, vector<const Variable*> >::const_iterator mi =
                variablesByRate.begin();
            for ( ; mi != variablesByRate.end(); ++mi) {
                double rate = mi->first;
                const vector<const Variable*>& vars = mi->second;

                /* Currently each sample received by the NetcdfRPCChannel
                 * must be from one station. Separate the requested
                 * variables by station and create a resampler for each station.
                 */
                map<int,vector<const Variable*> > varsByStn;
                for (unsigned int i = 0; i < vars.size(); i++) {
                    const Variable* var = vars[i];
                    int stn= var->getStation();
                    varsByStn[stn].push_back(vars[i]);
                }

                map<int,vector<const Variable*> >::const_iterator vi = varsByStn.begin();
                for ( ; vi != varsByStn.end(); ++vi) {
                    const vector<const Variable*> & dsmvars = vi->second;

                    if (rate > 0.0) {
                        NearestResamplerAtRate* smplr =
                            new NearestResamplerAtRate(dsmvars,false);
                        smplr->setRate(rate);
                        smplr->setFillGaps(true);
                        smplr->setMiddleTimeTags(_middleTimeTags);
                        ratesByResampler[smplr] = rate;
                        _resamplers.push_back(smplr);
                    }
                    else {
                        _resamplers.push_back(new NearestResampler(dsmvars,false));
                    }
                }
            }

            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                (*ri)->connect(pipeline.getProcessedSampleSource());
            }

            // Create a NetcdfRPCChannel instance from a dynamically loaded
            // shared object.  The instance is allocated dynamically and must
            // be deleted unless it is passed to NetcdfRPCOutput, in which
            // case NetcdfRPCOutput owns it and will delete it.  This throws
            // an exception if the shared object cannot be loaded.
            DOMable* domable = DOMObjectFactory::createObject("isff.NetcdfRPCChannel");
            nidas::core::IOChannel* ncchan = dynamic_cast<nidas::core::IOChannel*>(domable);

            // Create the equivalent XML configuration for the
            // NetcdfRPCChannel, so it can be configured through the virtual
            // fromDOMElement() method.

            //   <ncserver
            //      server="$NC_SERVER"
            //      dir="$NETCDF_DIR"
            //      file="$NETCDF_FILE"
            //      length="86400"
            //      cdl="$ISFS/projects/$PROJECT/$SYSTEM/config/$NETCDL_FILE"
            //      floatFill="1.e37"
            //      timeout="300"
            //      batchPeriod="300"/>
            std::ostringstream xml;
            xml << "<ncserver"
                << make_xatt("server", _ncserver)
                << make_xatt("dir", _ncdir)
                << make_xatt("file", _ncfile)
                << make_xatt("interval", _ncinterval)
                << make_xatt("length", _nclength)
                << make_xatt("cdl", _nccdl)
                << make_xatt("floatFill", _ncfill)
                << make_xatt("timeout", _nctimeout)
                << make_xatt("batchPeriod", _ncbatchperiod)
                << "/>";
            DLOG(("") << "created xml config for NetcdfRPCChannel: " << xml.str());

            std::unique_ptr<xercesc::DOMDocument> doc(XMLParser::ParseString(xml.str()));
            ncchan->fromDOMElement(doc->getDocumentElement());
            doc.reset();

            // nidas::dynld::isff::NetcdfRPCChannel* ncchan = new nidas::dynld::isff::NetcdfRPCChannel();
            // ncchan->setServer(_ncserver);
            // ncchan->setDirectory(_ncdir);
            // ncchan->setFileNameFormat(_ncfile);
            // ncchan->setTimeInterval(_ncinterval);
            // ncchan->setFileLength(_nclength);
            // ncchan->setCDLFileName(_nccdl);
            // ncchan->setFillValue(_ncfill);
            // ncchan->setRPCTimeout(_nctimeout);
            // ncchan->setRPCBatchPeriod(_ncbatchperiod);

            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                Resampler* smplr = *ri;
                std::list<const SampleTag*> tags = smplr->getSampleTags();
                std::list<const SampleTag*>::const_iterator ti = tags.begin();
                for ( ; ti != tags.end(); ++ti) {
                    const SampleTag *tp = *ti;
                    SampleTag tag(*tp);
                    tag.setRate(ratesByResampler[smplr]);
                    ncchan->addSampleTag(&tag);
                }
            }

            try {
                ncchan->connect();
            }
            catch (n_u::IOException& e) {
                for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                    (*ri)->disconnect(pipeline.getProcessedSampleSource());
                }
                pipeline.disconnect(&sis);
                sis.close();
                delete ncchan;
                throw e;
            }

            // Replace the value instance with a unique_ptr to an instance
            // allocated dynamically from a dynamically loaded shared object.

            // nidas::dynld::isff::NetcdfRPCOutput output(ncchan);
            DOMable* domchannel = DOMObjectFactory::createObject("isff.NetcdfRPCOutput");
            std::unique_ptr<SampleOutputBase> output
                (dynamic_cast<SampleOutputBase*>(domchannel));
            output->setIOChannel(ncchan);
            if (_clipping)
            {
                ILOG(("clipping netcdf output [")
                     << _startTime.format(true,"%Y %m %d %H:%M:%S")
                     << ","
                     << _endTime.format(true,"%Y %m %d %H:%M:%S")
                     << ")");
                output->setTimeClippingWindow(_startTime, _endTime);
            }

            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri)
                (*ri)->addSampleClient(output.get());

            try {
                if (_startTime.toUsecs() != 0) {
                    DLOG(("searching for time ") <<
                        _startTime.format(true,"%Y %m %d %H:%M:%S"));
                    sis.search(_startTime);
                    DLOG(("search done."));
                }
                for (;;) {
                    sis.readSamples();
                    if (_app.interrupted()) break;
                }
            }
            catch (n_u::EOFException& e) {
                cerr << "EOF received" << endl;
            }
            catch (n_u::IOException& e) {
                for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                    (*ri)->removeSampleClient(output.get());
                    (*ri)->disconnect(pipeline.getProcessedSampleSource());
                }
                pipeline.disconnect(&sis);
                sis.close();
                output->close();
                throw e;
            }

            pipeline.disconnect(&sis);
            sis.close();
            pipeline.flush();
            for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
                (*ri)->removeSampleClient(output.get());
            }
            output->close();
        }
        for (ri = _resamplers.begin() ; ri != _resamplers.end(); ++ri) {
            (*ri)->disconnect(pipeline.getProcessedSampleSource());
        }
        pipeline.interrupt();
        pipeline.join();
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
        cerr << "returning exception" << endl;
        return 1;
    }
    if (_app.interrupted()) return 1;       // interrupted
    return 0;
}

int main(int argc, char** argv)
{
    return DataPrep::main(argc,argv);
}
