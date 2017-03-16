/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#include "DSMServerApp.h"
#include "DSMServer.h"
#include "DSMServerIntf.h"
#include "StatusThread.h"
#include "DSMService.h"
#include "Site.h"
#include "ProjectConfigs.h"
#include "SampleOutputRequestThread.h"
#include "XMLParser.h"
#include "Version.h"

#include <nidas/util/Process.h>
#include <nidas/util/FileSet.h>
#include <nidas/util/Logger.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>  // MAXHOSTNAMELEN

#include <nidas/Config.h>

#ifdef HAVE_SYS_CAPABILITY_H 
#include <sys/capability.h>
#include <sys/prctl.h>
#endif

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

namespace {
    int defaultLogLevel = n_u::LOGGER_INFO;
};

/* static */
DSMServerApp* DSMServerApp::_instance = 0;

DSMServerApp::DSMServerApp():
    _debug(false),
    _xmlFileName(), _configsXMLName(),
    _rafXML("$PROJ_DIR/$PROJECT/$AIRCRAFT/nidas/flights.xml"),
    _isffXML("$ISFF/projects/$PROJECT/ISFF/config/configs.xml"),
    _isfsXML("$ISFS/projects/$PROJECT/ISFS/config/configs.xml"),
    _runState(RUN),
    _xmlrpcThread(0),_statusThread(0),
    _externalControl(false),
    _optionalProcessing(false),
    _signalMask(),
    _myThreadId(::pthread_self()),
    _datasetName(),
    _app("dsm_server")
{
    setupSignals();
}
DSMServerApp::~DSMServerApp()
{
    SampleOutputRequestThread::destroyInstance();
    SamplePools::deleteInstance();
}

int DSMServerApp::parseRunstring(int argc, char** argv)
{
    _app.setProcessName(argv[0]);

    NidasAppArg ExternalControl("-r,--remote", "",
                                "Enable XML-RPC server for remote control.");
    NidasAppArg OptionalProcessing("-o,--optional", ""
                                   "Run processors marked as optional in XML.");
    NidasAppArg DatasetName
        ("-S,--dataset", "<datasetname>",
         "Set environment variables specifed for the dataset\n"
         "as found in the xml file specifed by $NIDAS_DATASETS or \n"
         "$ISFS/projects/$PROJECT/ISFS/config/datasets.xml");

    _app.enableArguments(_app.DebugDaemon | _app.loggingArgs() |
                         _app.Version |
                         _app.Username |
                         _app.Hostname |
                         _app.DebugDaemon |
                         ExternalControl |
                         OptionalProcessing |
                         DatasetName);
    _app.parseArgs(ArgVector(argv+1, argv+argc));

    _externalControl = ExternalControl.asBool();
    _optionalProcessing = OptionalProcessing.asBool();
    _datasetName = DatasetName.getValue();

    int opt_char;		/* option character */
    while ((opt_char = getopt(argc, argv, "cdl:orS:u:h:v")) != -1) {
        switch (opt_char) {
        case 'c':
	    {
                const char* cfg = getenv("NIDAS_CONFIGS");
                if (cfg) _configsXMLName = cfg;
                else {
                    const char* re = getenv("PROJ_DIR");
                    const char* pe = getenv("PROJECT");
                    const char* ae = getenv("AIRCRAFT");
                    const char* ie = getenv("ISFS");
                    const char* ieo = getenv("ISFS");
                    if (re && pe && ae) _configsXMLName = n_u::Process::expandEnvVars(_rafXML);
                    else if (ie && pe) _configsXMLName = n_u::Process::expandEnvVars(_isfsXML);
                    else if (ieo && pe) _configsXMLName = n_u::Process::expandEnvVars(_isffXML);
                }
                if (_configsXMLName.length() == 0) {
                    cerr <<
                        "Environment variables not set correctly to find XML file of project configurations." << endl;
                    cerr << "Cannot find " << _rafXML << endl << "or " << _isfsXML << endl;
                    return usage(argv[0]);
                }
	    }
	    break;
        case '?':
            return usage(argv[0]);
        }
    }
    if (optind == argc - 1) _xmlFileName = string(argv[optind++]);
    else if (_configsXMLName.length() == 0) {
	const char* cfg = getenv("NIDAS_CONFIG");
	if (!cfg) {
	    cerr <<
		"Error: XML config file not found in runstring or $NIDAS_CONFIG" <<
	    endl;
            return usage(argv[0]);
        }
    	_xmlFileName = cfg;
    }
    return 0;
}

int DSMServerApp::usage(const char* argv0)
{
    const char* cfg;
    cerr << "\
Usage: " << argv0 << " [-c] [-d] [-l level] [-o] [-r] [-S dataSet_name] [-u username] [-v] [config]\n\
  -c: read configs XML file to find current project configuration, either\n\t" << 
    "\t$NIDAS_CONFIGS\nor\n\t" << _rafXML << "\nor\n\t" << _isfsXML << "\n\
  -d: debug, run in foreground and send messages to stderr with log level of debug\n\
      Otherwise run in the background, cd to /, and log messages to syslog\n\
      Specify a -l option after -d to change the log level from debug\n\
  -l loglevel: set logging level, 7=debug,6=info,5=notice,4=warning,3=err,...\n\
     The default level if no -d option is " << defaultLogLevel << "\n\
  -o: run processors marked as optional in XML\n\
  -r: rpc, start XML RPC thread to respond to external commands\n\
  -S dataSet_name: set environment variables specifed for the dataset\n\
     as found in the xml file specifed by $NIDAS_DATASETS or \n\
     $ISFS/projects/$PROJECT/ISFS/config/datasets.xml\n\
  -u username: after startup, switch userid to username\n\
  -v: display software version number and exit\n\
  config: (optional) name of DSM configuration file.\n\
    This parameter is not used if you specify the -c option\n\
    default: $NIDAS_CONFIG=\"" <<
	  	((cfg = getenv("NIDAS_CONFIG")) ? cfg : "<not set>") << "\"\n\
    Note: use an absolute path to this file if you run in the background without -d." << endl;
    return 1;
}
/* static */
int DSMServerApp::main(int argc, char** argv) throw()
{
    DSMServerApp app;

    int res = 0;
    if ((res = app.parseRunstring(argc,argv)) != 0) return res;

    app.initLogger();

    if ((res = app.initProcess()) != 0) return res;

    _instance = &app;

    try {

        // starts XMLRPC thread if -r runstring option
        app.startXmlRpcThread();

        res = app.run();

        app.killXmlRpcThread();
    }
    catch (const n_u::Exception &e) {
        PLOG(("%s",e.what()));
    }

    _instance = 0;

#ifdef DEBUG
    cerr << "XMLCachingParser::destroyInstance()" << endl;
#endif
    XMLCachingParser::destroyInstance();

#ifdef DEBUG
    cerr << "XMLImplementation::terminate()" << endl;
#endif
    XMLImplementation::terminate();

    return res;
}

void DSMServerApp::initLogger()
{
    _app.setupDaemon();
}

int DSMServerApp::initProcess()
{
    _app.setupProcess();

    return _app.checkPidFile();
}

int DSMServerApp::run() throw()
{
    int res = 0;
    SampleOutputRequestThread::getInstance()->start();
    for (; _runState != QUIT; ) {

        if (_runState == ERROR) {
            waitForSignal(15);
            if (_runState == QUIT) {
                res = 1;    // quit after an error. Return non-zero result
                break;
            }
            _runState = RUN;
        }

        Project project;

        try {

            /* If a dataset name has been passed, parse it and
             * set its environment variables from the datasets.xml file.
             */
            if (_datasetName.length() > 0) project.setDataset(getDataset());

            if (_configsXMLName.length() > 0) {
                ProjectConfigs configs;
                configs.parseXML(_configsXMLName);
                // throws InvalidParameterException if no config for time
                const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
                cfg->initProject(project);
                _xmlFileName = cfg->getXMLName();
            }
            else parseXMLConfigFile(_xmlFileName,project);
        }
	catch (const nidas::core::XMLException& e) {
	    PLOG(("%s",e.what()));
	    _runState = ERROR;
            continue;
	}
	catch(const n_u::InvalidParameterException& e) {
	    PLOG(("%s",e.what()));
	    _runState = ERROR;
            continue;
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
	    _runState = ERROR;
            continue;
	}

        project.setConfigName(_xmlFileName);

	DSMServer* server = 0;

	try {
	    char hostname[MAXHOSTNAMELEN];
	    gethostname(hostname,sizeof(hostname));

	    list<DSMServer*> servers =
	    	project.findServers(hostname);

	    if (servers.empty())
	    	throw n_u::InvalidParameterException("project","server",
			string("Can't find server entry for ") + hostname);
	    if (servers.size() > 1)
	    	throw n_u::InvalidParameterException("project","server",
			string("Multiple servers for ") + hostname);
	    server = servers.front();
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
	}
        if (_xmlrpcThread) _xmlrpcThread->setDSMServer(server);

        server->setXMLConfigFileName(_xmlFileName);

	try {
	    server->scheduleServices(_optionalProcessing);
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
            _runState = ERROR;
	}

        // start status thread if port is defined, via
        // <server statusAddr="sock::port"/>  in the configuration
        if (server->getStatusSocketAddr().getPort() != 0)
            startStatusThread(server);

        while (_runState == RUN) waitForSignal(0);

        killStatusThread();

        server->interruptServices();	

        server->joinServices();

        if (_xmlrpcThread) _xmlrpcThread->setDSMServer(0);

        // Project gets deleted here, which includes server.
    }
    return res;
}

void DSMServerApp::startXmlRpcThread() throw(n_u::Exception)
{
    if (!_externalControl) return;
    if (_xmlrpcThread) return;
    _xmlrpcThread = new DSMServerIntf();
    _xmlrpcThread->start();
}

void DSMServerApp::killXmlRpcThread() throw()
{
    if (!_xmlrpcThread) return;
    _xmlrpcThread->interrupt();

    try {
        DLOG(("DSMServer joining xmlrpcThread"));
       _xmlrpcThread->join();
        DLOG(("DSMServer xmlrpcThread joined"));
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
    delete _xmlrpcThread;
   _xmlrpcThread = 0;
}

void DSMServerApp::startStatusThread(DSMServer* server) throw(n_u::Exception)
{
    if (_statusThread) return;
    if (server->getStatusSocketAddr().getPort() != 0) {
        _statusThread = new DSMServerStat("DSMServerStat",server);
        _statusThread->start();
    }
}

void DSMServerApp::killStatusThread() throw()
{
    if (!_statusThread) return;

    try {
        if (_statusThread->isRunning()) {
            _statusThread->kill(SIGUSR1);
            DLOG(("kill(SIGUSR1) statusThread"));
        }
    }
    catch(const n_u::Exception& e) {
        WLOG(("statusThread: %s",e.what()));
    }
    try {
        DLOG(("DSMServer joining statusThread"));
        _statusThread->join();
        DLOG(("DSMServer statusThread joined"));
    }
    catch(const n_u::Exception& e) {
        WLOG(("statusThread: %s",e.what()));
    }
    delete _statusThread;
    _statusThread = 0;
}

void DSMServerApp::setupSignals()
{
    // unblock these in waitForSignal
    sigemptyset(&_signalMask);
    sigaddset(&_signalMask,SIGUSR1);
    sigaddset(&_signalMask,SIGHUP);
    sigaddset(&_signalMask,SIGTERM);
    sigaddset(&_signalMask,SIGINT);

    // block them otherwise
    pthread_sigmask(SIG_BLOCK,&_signalMask,0);
}

void DSMServerApp::waitForSignal(int timeoutSecs)
{
    // pause, unblocking the signals I'm interested in
    int sig;
    if (timeoutSecs > 0) {
        struct timespec ts = {timeoutSecs,0};
        sig = sigtimedwait(&_signalMask,0,&ts);
    }
    else sig = sigwaitinfo(&_signalMask,0);

    if (sig < 0) {
        if (errno == EAGAIN) return;    // timeout
        // if errno == EINTR, then the wait was interrupted by a signal other
        // than those that are unblocked here in _signalMask. This 
        // must have been an unblocked and non-ignored signal.
        if (errno == EINTR) PLOG(("DSMEngine::waitForSignal(): unexpected signal"));
        else PLOG(("DSMEngine::waitForSignal(): ") << n_u::Exception::errnoToString(errno));
        return;
    }

    ILOG(("DSMServer received signal ") << strsignal(sig) << '(' << sig << ')');

    switch(sig) {
    case SIGHUP:
	_runState = RESTART;
	break;
    case SIGTERM:
    case SIGINT:
	_runState = QUIT;
        break;
    case SIGUSR1:
        // an XMLRPC method could set _runState and send SIGUSR1
	break;
    default:
        WLOG(("sigtimedwait unknown signal:") << strsignal(sig));
        break;
    }
}

void DSMServerApp::parseXMLConfigFile(const string& xmlFileName,Project& project)
        throw(nidas::core::XMLException,n_u::InvalidParameterException,n_u::IOException)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();
    // throws nidas::core::XMLException

    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setXercesHandleMultipleImports(true);
    parser->setXercesDoXInclude(true);
    parser->setDOMDatatypeNormalization(false);

    // expand environment variables in name
    string expName = n_u::Process::expandEnvVars(xmlFileName);

    // Do not doc->release() this DOMDocument since it is
    // owned by the caching parser.
    NLOG(("parsing: ") << expName);
    xercesc::DOMDocument* doc = parser->parse(expName);
    // throws nidas::core::XMLException;

    project.fromDOMElement(doc->getDocumentElement());
    // throws n_u::InvalidParameterException;
}


Dataset DSMServerApp::getDataset() throw(n_u::InvalidParameterException, XMLException)
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
        if (ie && pe) XMLName = n_u::Process::expandEnvVars(isfsDatasetsXML);
        else if (ieo && pe) XMLName = n_u::Process::expandEnvVars(isffDatasetsXML);
    }

    if (XMLName.length() == 0)
        throw n_u::InvalidParameterException("environment variables",
            "NIDAS_DATASETS, ISFS, PROJECT","not found");
    Datasets datasets;
    datasets.parseXML(XMLName);

    Dataset dataset = datasets.getDataset(_datasetName);
    dataset.putenv();

    return dataset;
}

