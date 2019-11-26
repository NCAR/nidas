// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8; -*-
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
#include <nidas/core/NidasApp.h>

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

    static int main(int argc, char** argv) throw();

    int usage(const char* argv0);

    bool getFillGaps() const { return _fillGaps; }

    int listOutputSamples();

    Dataset getDataset() throw(n_u::InvalidParameterException, XMLException);

    void
    requireConfigsXML()
    {
        _configsXMLName = _app.getConfigsXML();
        if (_configsXMLName.length() == 0)
        {
            throw n_u::InvalidParameterException
                ("environment variables",
                 "PROJ_DIR,AIRCRAFT,PROJECT or ISFS,PROJECT",
                 "not found");
        }
    }

    StatisticsProcessor*
    getStatisticsProcessor(Project& project, const DSMConfig* & matchedDSM,
                           DSMServer* & matchedServer);

private:

    string _xmlFileName;

    string _dsmName;

    string _configName;

    static const int DEFAULT_PORT = 30000;

    float _sorterLength;

    bool _daemonMode;

    n_u::UTime _startTime;

    n_u::UTime _endTime;

    int _niceValue;

    static const int DEFAULT_PERIOD = 300;

    int _period;

    string _configsXMLName;

    bool _fillGaps;

    bool _doListOutputSamples;

    vector<unsigned int> _selectedOutputSampleIds;

    string _datasetName;

    NidasApp _app;
    NidasAppArg NiceValue;
    NidasAppArg SorterLength;
    NidasAppArg Period;
    NidasAppArg DaemonMode;
    NidasAppArg SetDSM;
    NidasAppArg DSMName;
    BadSampleFilterArg FilterArg;
};


int main(int argc, char** argv)
{
    return StatsProcess::main(argc, argv);
}


/* static */
int StatsProcess::main(int argc, char** argv) throw()
{
    NidasApp::setupSignals();

    StatsProcess stats;

    int res;
    
    // We want default log files of "level,message", so modify the default
    // scheme before parsing any log configs which might override it.
    n_u::LogScheme ls = n_u::Logger::getInstance()->getScheme();
    ls.setShowFields("level,message");
    n_u::Logger::getInstance()->setScheme(ls);

    try {
        res = stats.parseRunstring(argc,argv);
    }
    catch (NidasAppException& ex)
    {
        std::cerr << ex.what() << std::endl;
        res = 1;
    }
    if (res)
        return res;

    if (stats._daemonMode) {
        // fork to background, send stdout/stderr to /dev/null
        if (daemon(0,0) < 0) {
            n_u::IOException e("statsproc","daemon",errno);
            cerr << "Warning: " << e.toString() << endl;
        }
        n_u::Logger::createInstance("statsproc",LOG_PID,LOG_LOCAL5);
    }
    else n_u::Logger::createInstance(&cerr);

    if (stats._doListOutputSamples) return stats.listOutputSamples();

    return stats.run();
}

StatsProcess::StatsProcess():
    _xmlFileName(),_dsmName(),
    _configName(),
    _sorterLength(5.0),_daemonMode(false),
    _startTime(LONG_LONG_MIN),_endTime(LONG_LONG_MAX),
    _niceValue(0),_period(DEFAULT_PERIOD),
    _configsXMLName(),
    _fillGaps(false),_doListOutputSamples(false),
    _selectedOutputSampleIds(),_datasetName(),
    _app("statsproc"),
    NiceValue("-n,--nice", "<nice>",
              "Run at a lower priority (nice > 0)", "0"),
    SorterLength("-s,--sorterlength", "<seconds>",
                 "Input data sorter length in fractional seconds.", "5.0"),
    Period("-p,--period", "<seconds>",
           "Statistics period in seconds. If -S is used to specify a dataset,\n"
           "then the period is read from \n"
           "$ISFS/projects/$PROJECT/ISFS/config/datasets.xml.\n"
           "Otherwise it defaults to 300 seconds.", "300"),
    DaemonMode("-z,--daemon", "",
               "Run in daemon mode (in the background, log messages to syslog)"),
    SetDSM
    ("--DSM", "",
     "Set the DSM environment variable to the host name,\n"
     "as if the StatisticsProcessor were running in the\n"
     "context of a single DSM."),
    DSMName
    ("-d,--dsmname", "<dsmname>",
     "Look for a <fileset> belonging to the given dsm to "
     "determine input file names."),
    FilterArg()
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
    NidasApp& app = _app;

    app.allowUnrecognized(true);
    app.enableArguments(app.DatasetName | app.Hostname |
                        app.StartTime | app.EndTime | app.XmlHeaderFile |
                        app.InputFiles | Period | SorterLength |
                        NiceValue | DaemonMode | SetDSM | DSMName |
                        FilterArg |
                        app.loggingArgs() | app.Version | app.Help);
    app.StartTime.setFlags("-B,--start");
    app.EndTime.setFlags("-E,--end");
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    // statsproc does not have a true default input, but a partial socket
    // input can be specified, without the port, in which case the default
    // port will be used.
    app.InputFiles.setDefaultInput("", DEFAULT_PORT);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    _period = Period.asInt();
    _sorterLength = SorterLength.asFloat();
    _niceValue = NiceValue.asInt();
    _daemonMode = DaemonMode.asBool();
    _startTime = app.getStartTime();
    _endTime = app.getEndTime();
    _xmlFileName = app.xmlHeaderFile();
    _dsmName = DSMName.getValue();
    
    if (_sorterLength < 0.0 || _sorterLength > 1800.0)
    {
        throw NidasAppException("Invalid sorter length: " +
                                SorterLength.getValue());
    }
    if (_period < 0.0)
    {
        throw NidasAppException("Invalid period: " + Period.getValue());
    }

    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    NidasAppArgv left(argv[0], args);
    argc = left.argc;
    argv = left.argv;
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "c:fo:Oz")) != -1) {
        switch (opt_char) {
        case 'c':
            _configName = optarg;
            break;
        case 'f':
            _fillGaps = true;
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
                        _selectedOutputSampleIds.insert
                            (_selectedOutputSampleIds.end(),
                             ids.begin(),ids.end());
                    }
                    if (ic == string::npos) break;
                    soptarg = soptarg.substr(ic+1);
                }

                static n_u::LogContext lp(LOG_DEBUG);
                if (lp.active())
                {
                    n_u::LogMessage msg(&lp);
                    msg << "_selectedOutputSampleIds=";
                    for (unsigned int i = 0;
                         i < _selectedOutputSampleIds.size(); i++)
                        msg << _selectedOutputSampleIds[i] << ' ';
                }
            }
            break;
        case '?':
            return usage(argv[0]);
        }
    }
    app.parseInputs(left.unparsedArgs(optind));

    if (!_doListOutputSamples)
    {
        // must specify either:
        //  1. some data files to read, and optional begin and end times,
        //  2. a socket to connect to
        //  3. a time period and a $PROJECT environment variable
        //  3b a configuration name and a $PROJECT environment variable
        if (app.dataFileNames().size() == 0 && !app.socketAddress() &&
            _startTime.toUsecs() == LONG_LONG_MIN && _configName.length() == 0)
        {
            return usage(argv[0]);
        }

        if (_startTime.toUsecs() != LONG_LONG_MIN &&
            _endTime.toUsecs() == LONG_LONG_MAX)
        {
            _endTime = _startTime + 7 * USECS_PER_DAY;
        }
    }

    // This is a kludge to help situations where we want to run statsproc
    // on an individual DSM and generate output filenames or directories
    // using the DSM variable.  As a SampleIOProcessor, a
    // StatisticsProcessor can have a DSMConfig associated with it, in
    // which case strings are expanded in that context.  However, I think
    // think all the processors and output strings have been realized by
    // the time the project config has been parsed, so it is too late then
    // to set a DSMConfig.  We might be able to add a StatisticsProcessor
    // inside every <dsm> element, and then even take advantage of the -d
    // option to run that specific DSM's stats processor, but that means
    // generating a stats xml file for every DSM, and each with a unique
    // starting sample ID.
    if (SetDSM.asBool())
    {
        std::string sdsm("DSM=");
        sdsm += _app.getShortHostName();
        char* dsm = new char[sdsm.length()+1];
        strcpy(dsm, sdsm.c_str());
        putenv(dsm);
    }
    return 0;
}

/* static */
int StatsProcess::usage(const char* argv0)
{
    cerr <<
        "Usage: " << argv0 << " [options] [-c configName] [-d dsmname] [-f]\n"
        "    [-S dataSet_name] [input ...]\n"
        "    -c configName: (optional) \n"
        "         name of configuration period to process, from configs.xml\n"
        "    -f: Fill in time gaps with missing data. When reprocessing data\n"
        "        you probably want to set this option.  If for some reason \n"
        "        you were reprocessing separate time periods in one run, \n"
        "        or if some of the archive files are missing, then you may not\n"
        "        want statsproc to output missing data values to the netcdf\n"
        "        files for the skipped time periods, and so then should omit -f.\n"
        "        If the netcdf files are being created in this run, then -f\n"
        "        is unnecessary.\n"
        "    -O: list sample ids of StatisticsProcessor output samples\n"
        "    -o [i] | [i-j] [,...]:\n"
        "        select ids of output samples of StatisticsProcessor for\n"
        "        processing.\n"
        "\n"
        "If no inputs are specified, then the -B time option must be given,\n" <<
        "and " << argv0 << " will read \n"
        "$ISFS/projects/$PROJECT/ISFS/config/configs.xml, to find an xml\n"
        "configuration for the begin time, read it to find a <fileset> archive,\n"
        "and then open data files matching the <fileset> path\n"
        "descriptor and time period.  Use the -d <dsmname> option to specify\n"
        "a DSM in which to look for the <output>, otherwise a server entry is\n"
        "searched for the fileset.\n" 
        "\n"
        "The default XML file name is determined by either reading the data\n"
        "file header or from $ISFS/projects/$PROJECT/ISFS/config/configs.xml\n"
        "\n" <<
        argv0 << " scans the xml file for a <processor> of class\n"
        "StatisticsProcessor in order to determine what statistics to generate.\n"
        "\n"
        "Standard nidas options:\n" << _app.usage() << "\n"
        "Examples:\n" <<
        argv0 << " --start \"2006 jun 10 00:00\" --end \"2006 jul 3 00:00\"\n" <<
        argv0 << " sock:dsmhost\n" <<
        argv0 << " unix:/tmp/data_socket\n" <<
        endl;
    return 1;
}


Dataset StatsProcess::getDataset() throw(n_u::InvalidParameterException,
                                         XMLException)
{
    string XMLName;
    const char* ndptr = getenv("NIDAS_DATASETS");

    if (ndptr) XMLName = string(ndptr);
    else {
        const char* isffDatasetsXML =
            "$ISFF/projects/$PROJECT/ISFF/config/datasets.xml";
        const char* isfsDatasetsXML =
            "$ISFS/projects/$PROJECT/ISFS/config/datasets.xml";
        const char* ie = ::getenv("ISFS");
        const char* ieo = ::getenv("ISFF");
        const char* pe = ::getenv("PROJECT");
        if (ie && pe)
            XMLName = n_u::Process::expandEnvVars(isfsDatasetsXML);
        else if (ieo && pe)
            XMLName = n_u::Process::expandEnvVars(isffDatasetsXML);
    }

    if (XMLName.length() == 0)
        throw n_u::InvalidParameterException("environment variables",
            "NIDAS_DATASETS, ISFS, PROJECT","not found");
    Datasets datasets;
    datasets.parseXML(XMLName);

    Dataset dataset = datasets.getDataset(_datasetName);
    dataset.putenv();

    _period = dataset.getResolutionSecs();

    return dataset;
}

int StatsProcess::run() throw()
{
    if (_niceValue > 0 && nice(_niceValue) < 0)
    {
        WLOG(("") << _app.getProcessName()
             << ": nice(" << _niceValue << "): "
             << strerror(errno));
        return 1;
    }

    try {

        Project project;

        if (_datasetName.length() > 0) project.setDataset(getDataset());

        IOChannel* iochan = 0;

        if (_xmlFileName.length() > 0) {
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            n_u::auto_ptr<xercesc::DOMDocument> doc
                (nidas::core::parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }
        XMLImplementation::terminate();

        if (_app.socketAddress())
        {
            if (_xmlFileName.length() == 0)
            {
                requireConfigsXML();

                ProjectConfigs configs;
                configs.parseXML(_configsXMLName);
                ILOG(("parsed:") <<  _configsXMLName);
                // throws InvalidParameterException if no config for time
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
                catch(const n_u::IOException& e)
                {
                    WLOG(("") << e.what() << ": retrying in 10 seconds");
                    sleep(10);
                }
            }
            if (_app.interrupted())
            {
                return 1;
            }
            sock->setKeepAliveIdleSecs(60);
            iochan = new nidas::core::Socket(sock);
            ILOG(("connected: ") <<  sock->getRemoteSocketAddress().toString());
        }
        else {
            nidas::core::FileSet* fset;

            // no file names listed in runstring
            if (_app.dataFileNames().size() == 0)
            {
                // User has not specified the xml file. Get
                // the ProjectConfig from the configName or startTime
                // using the configs XML file, then parse the
                // XML of the ProjectConfig.
                if (_xmlFileName.length() == 0)
                {
                    requireConfigsXML();
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

                    if (_startTime.toUsecs() == LONG_LONG_MIN)
                        _startTime = cfg->getBeginTime();
                    if (_endTime.toUsecs() == LONG_LONG_MAX)
                        _endTime = cfg->getEndTime();
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

                if (_startTime.toUsecs() != LONG_LONG_MIN)
                    fset->setStartTime(_startTime);
                if (_endTime.toUsecs() != LONG_LONG_MAX)
                    fset->setEndTime(_endTime);
            }
            else
            {
                fset = nidas::core::FileSet::getFileSet(_app.dataFileNames());
            }
            iochan = fset;
        }

        RawSampleInputStream sis(iochan);
        BadSampleFilter& bsf = FilterArg.getFilter();
        bsf.setDefaultTimeRange(_startTime, _endTime);
        sis.setBadSampleFilter(bsf);

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
            n_u::auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }

        project.setConfigName(_xmlFileName);

        StatisticsProcessor* sproc = 0;
        const DSMConfig* dsm = 0;
        DSMServer* server = 0;

        sproc = getStatisticsProcessor(project, dsm, server);
        if (!sproc)
        {
            return 1;
        }
        if (dsm) {
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
        }
        else if (_dsmName.length() > 0) {
            PLOG(("Cannot find dsm \"%s\"", _dsmName.c_str()));
            return 1;
        }

        if (server) {
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
        }

        sproc->setFillGaps(getFillGaps());

        if (_selectedOutputSampleIds.size() > 0)
            sproc->selectRequestedSampleTags(_selectedOutputSampleIds);

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

            if (_app.socketAddress())
            {
                SampleOutputRequestThread::getInstance()->start();
            }
            else
            {
                const std::list<SampleOutput*>& outputs = sproc->getOutputs();
                std::list<SampleOutput*>::const_iterator oi = outputs.begin();
                for ( ; oi != outputs.end(); ++oi) {
                    SampleOutput* output = *oi;
                    output->requestConnection(sproc);
                }
            }

            for (;;) {
                if (_app.interrupted()) break;
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

StatisticsProcessor*
StatsProcess::
getStatisticsProcessor(Project& project, const DSMConfig* & matchedDSM,
                       DSMServer* & matchedServer)
{
    StatisticsProcessor* sproc = 0;

    const DSMConfig* dsm = 0;
    // If dsmName is set, then look in that specific DSM first for a
    // matching StatisticsProcessor instance.
    if (_dsmName.length() > 0) {
        dsm = project.findDSM(_dsmName);
        if (dsm) {
            ProcessorIterator pitr = dsm->getProcessorIterator();
            for ( ; pitr.hasNext(); ) {
                SampleIOProcessor* proc = pitr.next();
                StatisticsProcessor* sp = 0;
                sp = dynamic_cast<StatisticsProcessor*>(proc);
                if (!sp) continue;
                VLOG(("") << "sp period=" << sp->getPeriod()
                     << " _period=" << _period
                     << " period diff=" << (sp->getPeriod() - _period)
                     << " equality=" << (sp->getPeriod() == _period));
                if (fabs(sp->getPeriod()-_period) < 1.e-3) {
                    sproc = sp;
                    matchedDSM = dsm;
                    break;
                }
            }
        }
    }
    if (!sproc)
    {
        // Find a server with a StatisticsProcessor
        // If no match is found, the name up to the first
        // dot is tried. If still no match, returns
        // servers in the project with no associated name.
        list<DSMServer*> servers = project.findServers(_app.getHostName());

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
                    matchedServer = server;
                    break;
                }
            }
        }
    }
    if (!sproc)
    {
        if (_dsmName.length())
        {
            PLOG(("Cannot find a StatisticsProcessor for dsm '%s' "
                  "or server '%s' with period=%d",
                  _dsmName.c_str(), _app.getHostName().c_str(), _period));
        }
        else
        {
            PLOG(("Cannot find a StatisticsProcessor for server '%s' "
                  "with period=%d",
                  _app.getHostName().c_str(), _period));
        }
    }
    return sproc;
}

int StatsProcess::listOutputSamples()
{
    try {

        Project project;

        if (_xmlFileName.length() > 0) {
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            n_u::auto_ptr<xercesc::DOMDocument> doc(nidas::core::parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }
        else
        {
            requireConfigsXML();
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

        StatisticsProcessor* sproc;
        const DSMConfig* dsm = 0;
        DSMServer* server = 0;

        sproc = getStatisticsProcessor(project, dsm, server);
        if (!sproc)
        {
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
