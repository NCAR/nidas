
#include "NidasApp.h"
#include "Project.h"
#include "Version.h"

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include <iomanip>

using std::string;

using namespace nidas::core;
namespace n_u = nidas::util;
using nidas::util::Logger;
using nidas::util::LogScheme;

using namespace std;

namespace 
{
  int
  int_from_string(const std::string& text)
  {
    int num;
    errno = 0;
    char* endptr;
    num = strtol(text.c_str(), &endptr, 0);
    if (text.empty() || errno != 0 || *endptr != '\0')
    {
      throw std::invalid_argument(text);
    }
    return num;
  }
}


NidasApp::
NidasApp(const std::string& name) :
  XmlHeaderFile
  ("-x", "--xml",
   "Path to the NIDAS XML header file.  The default path is\n"
   "taken from the header and expanded "
   "using the current environment settings.",
   "<xmlfile>"),
  LogShow
  ("", "--logshow",
   "As log points are created, show information for each one that can\n"
   "be used to enable log messages from that log point."),
  LogConfig
  ("-l", "--logconfig",
   "Add a log config to the log scheme.  The log config settings are\n"
   "specified as a comma-separated list of fields, using syntax \n"
   "<field>=<value>, where fields are tag, file, function, line, enable,\n"
   "and disable.\n"
   "The log level can be specified as either a number or string: \n"
   "7=debug,6=info,5=notice,4=warning,3=error,2=critical. Default is info.",
   "<loglevel>"),
  LogLevel
  ("-l", "--loglevel", "Alias for --logconfig.", "<loglevel>"),
  LogFields
  ("", "--logfields",
   "Set the log fields to be shown in log messages, as a comma-separated list\n"
   "of log field names: thread,function,file,level,time,message.\n"
   "The default log message fields are these: time,level,message",
   "<logfields>"),
  LogParam
  ("", "--logparam",
   "Set a log scheme parameter from syntax <name>=<value>.",
   "<name>=<value>"),
  Help
  ("-h", "--help", "Print usage information."),
  ProcessData
  ("-p", "--process", "Enable processed samples rather than raw samples."),
  StartTime
  ("-s", "--start",
   "Skip samples until start-time, in the form '2006 Apr 1 00:00'",
   "<start-time>"),
  EndTime
  ("-e", "--end",
   "Skip samples after end-time, in the form '2006 Apr 1 00:00'",
   "<end-time>"),
  SampleRanges
  ("-i", "--samples", 
   "D is a dsm id or range of dsm ids separated by '-', or * (or -1) for all.\n"
   "S is a sample id or range of sample ids separated by '-', "
   "or * (or -1) for all.\n"
   "Sample ids can be specified in 0x hex format with a leading 0x, in which\n"
   "case they will also be output in hex.\n"
   "Prefix the range option with ^ to exclude that range of samples.\n"
   "Multiple range options can be specified.  Samples are either included\n"
   "or excluded according to the first option which matches their ID.\n"
   "If only exclusions are specified, then all other samples are implicitly\n"
   "included.\n"
   "Use data_stats to see DSM ids and sample ids in a data file.\n"
   "More than one sample range can be specified.\n"
   "Examples: \n"
   " -i ^1,-1     Include all samples except those with DSM ID 1.\n"
   " -i '^5,*' --samples 1-10,1-2\n"
   "              Include sample IDs 1-2 for DSMs 1-10 except for DSM 5.\n",
   "[^]{<d1>[-<d2>|*},{<s1>[-<s2>]|*}"),
  Version
  ("-v", "--version", "Print version information and exit."),
  InputFiles(),
  OutputFiles
  ("-o", "--output",
   "Specify a file pattern for output files using strptime() substitutions.\n"
   "The path can optionally be followed by a file length and units:\n"
   "hours (h), minutes (m), and seconds (s). The default is seconds.\n"
   "nidas_%Y%m%d_%H%M%S.dat@30m generates files every 30 minutes.\n",
   "<strptime_path>[@<number>[units]]"),
  _appname(name),
  _processData(false),
  _xmlFileName(),
  _idFormat(DECIMAL),
  _sampleMatcher(),
  _startTime(LONG_LONG_MIN),
  _endTime(LONG_LONG_MAX),
  _dataFileNames(),
  _sockAddr(),
  _outputFileName(),
  _outputFileLength(0),
  _help(false),
  _deleteProject(false)
{
  enableArguments(LogShow | LogFields);

  // We want to setup a "default" LogScheme which will be overridden if any
  // other log configs are explicitly added through this NidasApp.  So
  // create our own scheme with a reserved name, and then that scheme will
  // be replaced if a new one is created with the app name of this NidasApp
  // instance.  If a named scheme has already been set as the current
  // scheme, then do not replace it.
  n_u::LogConfig lc;
  Logger* logger = Logger::getInstance();
  // Fetch the scheme instead of creating it from scratch in case one was
  // already installed and modified.
  LogScheme scheme = logger->getScheme("NidasAppDefault");
  lc.level = n_u::LOGGER_INFO;
  scheme.addConfig(lc);
  if (logger->getScheme().getName() == n_u::LogScheme().getName())
  {
    logger->setScheme(scheme);
  }
}


NidasApp::
~NidasApp()
{
  // If the global Project instance was retrieved through this NidasApp
  // instance, and if this is the application-wide instance of NidasApp,
  // then this is the "owner" of the Project instance and it must be safe
  // to destroy it here.
  if (this == application_instance)
  {
    application_instance = 0;
    if (_deleteProject)
    {
      Project::destroyInstance();
      _deleteProject = false;
    }
  }
}


NidasApp* 
NidasApp::application_instance = 0;


void
NidasApp::
setApplicationInstance()
{
  application_instance = this;
}


NidasApp*
NidasApp::
getApplicationInstance()
{
  return application_instance;
}


void
NidasApp::
parseLogConfig(const std::string& optarg) throw (NidasAppException)
{
  // Create a LogConfig from this argument and add it to the current scheme.
  n_u::LogConfig lc;
  Logger* logger = Logger::getInstance();
  LogScheme scheme = logger->getScheme(getName());

  if (!lc.parse(optarg))
  {
    throw NidasAppException("error parsing log level: " + string(optarg));
  }
  scheme.addConfig(lc);
  logger->setScheme(scheme);
}


std::string
xarg(std::vector<std::string>& args, int i)
{
  if (i < (int)args.size())
  {
    return args[i];
  }
  throw NidasAppException("expected argument for option " + args[i-1]);
}


nidas::util::UTime
NidasApp::
parseTime(const std::string& optarg)
{
  nidas::util::UTime ut;
  try {
    ut = n_u::UTime::parse(true, optarg);
  }
  catch (const n_u::ParseException& pe) {
    throw NidasAppException("could not parse time " + optarg +
			    ": " + pe.what());
  }
  return ut;
}


void
NidasApp::
parseArguments(std::vector<std::string>& args) throw (NidasAppException)
{
  int i = 0;
  while (i < (int)args.size())
  {
    std::string arg = args[i];
    int istart = i;
    bool handled = true;

    if (XmlHeaderFile.accept(arg))
    {
      _xmlFileName = xarg(args, ++i);
    }
    else if (SampleRanges.accept(arg))
    {
      std::string optarg = xarg(args, ++i);
      if (! _sampleMatcher.addCriteria(optarg))
      {
	throw NidasAppException("sample criteria could not be parsed: " +
				optarg);
      }
      if (optarg.find("0x", 0) != string::npos)
      {
	_idFormat = HEX_ID;
      }
    }
    else if (LogLevel.accept(arg))
    {
      parseLogConfig(xarg(args, ++i));
    }
    else if (LogConfig.accept(arg))
    {
      parseLogConfig(xarg(args, ++i));
    }
    else if (LogFields.accept(arg))
    {
      Logger* logger = Logger::getInstance();
      LogScheme scheme = logger->getScheme(getName());
      scheme.setShowFields(xarg(args, ++i));
      logger->setScheme(scheme);
    }
    else if (LogParam.accept(arg))
    {
      Logger* logger = Logger::getInstance();
      LogScheme scheme = logger->getScheme(getName());
      scheme.parseParameter(xarg(args, ++i));
      logger->setScheme(scheme);
    }
    else if (LogShow.accept(arg))
    {
      Logger* logger = Logger::getInstance();
      LogScheme scheme = logger->getScheme(getName());
      scheme.showLogPoints(true);
      logger->setScheme(scheme);
    }
    else if (ProcessData.accept(arg))
    {
      _processData = true;
    }
    else if (EndTime.accept(arg))
    {
      _endTime = parseTime(xarg(args, ++i));
      _sampleMatcher.setEndTime(_endTime);
    }
    else if (StartTime.accept(arg))
    {
      _startTime = parseTime(xarg(args, ++i));
      _sampleMatcher.setStartTime(_startTime);
    }
    else if (OutputFiles.accept(arg))
    {
      parseOutput(xarg(args, ++i));
    }
    else if (Version.accept(arg))
    {
      std::cout << "Version: " << Version::getSoftwareVersion() << std::endl;
      exit(0);
    }
    else if (Help.accept(arg))
    {
      _help = true;
    }
    else
    {
      handled = false;
      ++i;
    }
    if (handled)
    {
      // Remove arguments [istart, i]
      args.erase(args.begin() + istart, args.begin() + i + 1);
      i = istart;
    }
  }
}


void
NidasApp::
parseInputs(std::vector<std::string>& inputs,
	    std::string default_input,
	    int default_port) throw (NidasAppException)
{
  if (default_input.length() == 0)
  {
    default_input = InputFiles.default_input;
  }
  if (default_port == 0)
  {
    default_port = InputFiles.default_port;
  }
  if (inputs.empty() && default_input.length() > 0)
  {
    inputs.push_back(default_input);
  }

  for (unsigned int i = 0; i < inputs.size(); i++) {
    string url = inputs[i];
    if (url.length() > 5 && url.substr(0,5) == "sock:") {
      url = url.substr(5);
      size_t ic = url.find(':');
      int port = default_port;
      string hostName = url.substr(0,ic);
      if (ic < string::npos) {
	std::istringstream ist(url.substr(ic+1));
	ist >> port;
	if (ist.fail()) {
	  throw NidasAppException("Invalid port number: " + url.substr(ic+1));
	}
      }
      try {
	n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
	_sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
      }
      catch(const n_u::UnknownHostException& e) {
	throw NidasAppException(e.what());
      }
    }
    else if (url.length() > 5 && !url.compare(0,5,"unix:")) {
      url = url.substr(5);
      _sockAddr.reset(new nidas::util::UnixSocketAddress(url));
    }
    else
      _dataFileNames.push_back(url);
  }
}


void
NidasApp::
parseOutput(const std::string& optarg) throw (NidasAppException)
{
  std::string output = optarg;
  std::string slen;
  int factor = 1;
  int length = 0;

  size_t ic = optarg.rfind("@");
  if (ic != string::npos)
  {
    output = optarg.substr(0, ic);
    slen = optarg.substr(ic+1);
    if (slen.empty())
    {
      throw NidasAppException("missing length specifier after @: " + optarg);
    }
    size_t uend = slen.length() - 1;
    if (*slen.rbegin() == 's')
      factor = 1;
    else if (*slen.rbegin() == 'm')
      factor = 60;
    else if (*slen.rbegin() == 'h')
      factor = 3600;
    else
      ++uend;
    try {
      length = int_from_string(slen.substr(0, uend)) * factor;
    }
    catch (std::invalid_argument& err)
    {
      throw NidasAppException("invalid length specifier: " + optarg);
    }
    _outputFileLength = length;
  }
  _outputFileName = output;
}


namespace
{
  void (*app_interrupted_callback)() = 0;

  bool app_interrupted = false;

  void sigAction(int sig, siginfo_t* siginfo, void*)
  {
    std::cerr <<
      "received signal " << strsignal(sig) << '(' << sig << ')' <<
      ", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
      ", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
      ", si_code=" << (siginfo ? siginfo->si_code : -1) << std::endl;
                                                                                
    switch(sig)
    {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
      app_interrupted = true;
      if (app_interrupted_callback)
	(*app_interrupted_callback)();
      break;
    }
  }
}


bool
NidasApp::
interrupted()
{
  return app_interrupted;
}


/* static */
void
NidasApp::
setupSignals(void (*callback)())
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
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
    app_interrupted_callback = callback;
}


nidas_app_arglist_t
NidasApp::
loggingArgs()
{
  nidas_app_arglist_t args = 
    LogShow | LogConfig | LogLevel | LogFields | LogParam;
  return args;
}


nidas_app_arglist_t
nidas::core::
operator|(nidas_app_arglist_t arglist1, nidas_app_arglist_t arglist2)
{
    std::copy(arglist2.begin(), arglist2.end(), std::back_inserter(arglist1));
    return arglist1;
}


std::string
NidasApp::
usage()
{
  std::ostringstream oss;

  // Iterate through a list of all the known argument types, dumping 
  // usage info only for those which are enabled.  The general format
  // is this:
  //
  // [<shortflag>,]<longflag> [<spec>]
  // Description

  nidas_app_arglist_t args = 
    XmlHeaderFile |
    LogShow | LogConfig | LogLevel | LogFields | LogParam |
    Help | ProcessData | StartTime | EndTime |
    SampleRanges | Version | InputFiles | OutputFiles;

  nidas_app_arglist_t::iterator it;
  for (it = args.begin(); it != args.end(); ++it)
  {
    NidasAppArg& arg = (**it);
    if (arg.enabled)
    {
      if (arg.enableShortFlag && arg.flag.size())
      {
	oss << arg.flag;
	if (!arg.longFlag.empty())
	  oss << ",";
      }
      if (!arg.longFlag.empty())
	oss << arg.longFlag;
      if (!arg.specifier.empty())
	oss << " " << arg.specifier;
      oss << "\n";
      std::string text = arg.usage();
      oss << arg.usage() << "\n";
      if (text.length() && text[text.length()-1] != '\n')
      {
	oss << "\n";
      }
    }
  }
  return oss.str();
}


Project*
NidasApp::
getProject()
{
  _deleteProject = true;
  return Project::getInstance();
}


void
NidasAppInputFilesArg::
updateUsage()
{
  std::ostringstream oss;
  oss << "input-url: One of the following:\n";
  if (allowSockets && default_port > 0)
  {
    oss << "  sock:host[:port]    (Default port is " << default_port
	<< ")\n";
  }
  if (allowSockets)
  {
    oss << "  unix:sockpath       unix socket name\n";
  }
  if (allowFiles)
  {
    oss << "  path [...]          file names\n";
  }
  oss << "Default inputURL is \"sock:localhost\"\n";
  setUsageString(oss.str());
}

void
NidasApp::
setIdFormat(id_format_t idt)
{
  _idFormat = idt;
}


std::ostream&
NidasApp::
formatSampleId(std::ostream& leader, id_format_t idFormat, dsm_sample_id_t sampid)
{
  int dsmid = GET_DSM_ID(sampid);
  int spsid = GET_SHORT_ID(sampid);

  leader << setw(2) << setfill(' ') << dsmid << ',';
  switch(idFormat) {
  case NidasApp::HEX_ID:
    leader << "0x" << setw(4) << setfill('0') << hex << spsid << dec << ' ';
    break;
#ifdef SUPPORT_OCTAL_IDS
  case NidasApp::OCTAL:
    leader << "0" << setw(6) << setfill('0') << oct << spsid << dec << ' ';
    break;
#else
  default:
#endif
  case NidasApp::DECIMAL:
    leader << setw(4) << spsid << ' ';
    break;
  }
  return leader;
}


int
NidasApp::
logLevel()
{
  Logger* logger = Logger::getInstance();
  // Just return the logLevel() of the current scheme, whether that's
  // the default scheme or one set explicitly through this NidasApp.
  return logger->getScheme().logLevel();
}

void
NidasApp::
resetLogging()
{
  // Reset logging to the NidasApp default scheme (with the default log
  // level) and reset the user-configured scheme too.
  LogScheme scheme = n_u::LogScheme("NidasAppDefault");
  n_u::LogConfig lc;
  lc.level = n_u::LOGGER_INFO;
  scheme.addConfig(lc);
  Logger::getInstance()->updateScheme(LogScheme(getName()));
  Logger::getInstance()->setScheme(scheme);
}
