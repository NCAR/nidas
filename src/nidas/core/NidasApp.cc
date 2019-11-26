// -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "NidasApp.h"
#include "Project.h"
#include "Version.h"

#include <nidas/util/Process.h>
#include <nidas/util/FileSet.h>

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <pwd.h>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include <iomanip>

#ifdef HAVE_SYS_CAPABILITY_H 
#include <sys/prctl.h>
#endif 

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

  float
  float_from_string(const std::string& text)
  {
    float num;
    errno = 0;
    char* endptr;
    num = strtof(text.c_str(), &endptr);
    if (text.empty() || errno != 0 || *endptr != '\0')
    {
      throw std::invalid_argument(text);
    }
    return num;
  }

}


NidasAppArg::
NidasAppArg(const std::string& flags,
	    const std::string& syntax,
	    const std::string& usage,
	    const std::string& default_,
            bool required) :
  _flags(flags),
  _syntax(syntax),
  _usage(usage),
  _default(default_),
  _arg(),
  _value(),
  _enableShortFlag(true),
  _required(required)
{}


NidasAppArg::
~NidasAppArg()
{}


void
NidasAppArg::
setRequired(const bool isRequired)
{
  _required = isRequired;
}


bool
NidasAppArg::
isRequired()
{
  return _required;
}


bool
NidasAppArg::
specified()
{
  return !_arg.empty();
}


const std::string&
NidasAppArg::
getValue()
{
  if (specified())
  {
    return _value;
  }
  return _default;
}


const std::string&
NidasAppArg::
getFlag()
{
  return _arg;
}


void
NidasAppArg::
addFlag(const std::string& flag)
{
  if (_flags.length())
  {
    _flags += ",";
  }
  _flags += flag;
}


void
NidasAppArg::
setFlags(const std::string& flags)
{
  _flags = flags;
}


bool
NidasAppArg::
asBool()
{
  std::string value = getValue();
  bool result = false;

  if (value == "yes" || value == "on" || value == "true")
  {
    result = true;
  }
  else if (value != "" && value != "no" && value != "off" && value != "false")
  {
    std::ostringstream msg;
    msg << "Value of " << _flags << " must be one of these: "
        << "on,off,yes,no,true,false";
    throw NidasAppException(msg.str());
  }
  return result;
}


int
NidasAppArg::
asInt()
{
  try {
    return int_from_string(getValue());
  }
  catch (const std::invalid_argument& ex)
  {
    std::ostringstream msg;
    msg << "Value of " << _flags << " is not an integer: " << getValue();
    throw NidasAppException(msg.str());
  }
#ifdef notdef
  std::istringstream ist(getValue());
  ist >> result;
  if (ist.fail())
  {
    std::ostringstream msg;
    msg << "Value of " << _flags << " is not an integer: " << getValue();
    throw NidasAppException(msg.str());
  }
  return result;
#endif
}


float
NidasAppArg::
asFloat()
{
  try {
    return float_from_string(getValue());
  }
  catch (const std::invalid_argument& ex)
  {
    std::ostringstream msg;
    msg << "Value of " << _flags << " is not a float: " << getValue();
    throw NidasAppException(msg.str());
  }
#ifdef notdef
  float result;
  std::istringstream ist(getValue());
  ist >> result;
  if (ist.fail())
  {
    std::ostringstream msg;
    msg << "Value of " << _flags << " is not a float: " << getValue();
    throw NidasAppException(msg.str());
  }
  return result;
#endif
}


bool
NidasAppArg::
parse(const ArgVector& argv, int* argi)
{
  bool result = false;
  int i = 0;
  if (argi)
    i = *argi;
  std::string flag = argv[i];
  if (accept(flag))
  {
    if (!single())
    {
      _value = expectArg(argv, ++i);
    }
    _arg = flag;
    result = true;
  }
  if (argi)
    *argi = i;
  return result;
}


bool
NidasAppArg::
single()
{
  return _syntax.length() == 0;
}


bool
NidasAppArg::
accept(const std::string& arg)
{
  // Ignore brackets in the flags indicating deprecated flags.
  size_t start = 0;
  string flags = _flags;
  string::size_type bracket = string::npos;
  while ((bracket = flags.find_first_of("[]")) != string::npos)
    flags.erase(bracket, 1);
  while (start < flags.length())
  {
    size_t comma = flags.find(',', start);
    if (comma == std::string::npos)
      comma = flags.length();
    string flag = flags.substr(start, comma-start);
    if (flag == arg && (flag.length() > 2 || _enableShortFlag))
    {
      if (single())
      {
        // Specifying this form of a single boolean flag sets it to true,
        // no matter what.
        _value = "true";
      }
      return true;
    }
    // The negative form is only checked if there is a non-empty default.
    if (single() && _default.length() && flag.substr(0, 2) == "--")
    {
      string negflag = "--no-" + flag.substr(2);
      if (flag.length() > 2 && arg == negflag)
      {
        // The negative flag sets the value to false.
        _value = "false";
      }
    }
    start = comma+1;
  }
  return false;
}
  

std::string
NidasAppArg::
getUsageFlags()
{
  // Extract the accepted flags for usage info.  This excludes short flags
  // if disabled and deprecated flags surrounded by brackets.
  std::string flags = _flags;
  string::size_type left = _flags.find('[');
  string::size_type right = _flags.find(']');
  if (left != string::npos && right != string::npos)
    flags.erase(left, right);
  size_t start = 0;
  std::string uflags;
  while (start < flags.length())
  {
    size_t comma = flags.find(',', start);
    if (comma == std::string::npos)
      comma = flags.length();
    if (comma - start > 2 || _enableShortFlag)
    {
      if (uflags.length())
	uflags += ",";
      string flag = flags.substr(start, comma-start);
      if (single() && _default.length() && flag.substr(0, 2) == "--")
        flag = "--[no-]" + flag.substr(2);
      uflags += flag;
    }
    start = comma+1;
  }
  return uflags;
}
  

std::string
NidasAppArg::
usage(const std::string& indent)
{
  std::ostringstream oss;
  string flags = getUsageFlags();
  oss << indent << flags;
  if (!_syntax.empty())
  {
    if (!flags.empty())
      oss << " ";
    oss << _syntax;
  }
  if (!_default.empty())
    oss << " [default: " << _default << "]";
  oss << "\n";

  std::istringstream iss(_usage);
  std::string line;
  while (getline(iss, line))
  {
    oss << indent << indent << line << "\n";
  }
  oss << "\n";
  return oss.str();
}



namespace
{
  const char* RAFXML = "$PROJ_DIR/$PROJECT/$AIRCRAFT/nidas/flights.xml";
  const char* ISFFXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";
  const char* ISFSXML = "$ISFS/projects/$PROJECT/ISFS/config/configs.xml";
}

std::string
NidasApp::
getConfigsXML()
{
  std::string configsXMLName;
  const char* cfg = getenv("NIDAS_CONFIGS");
  if (cfg)
  {
    // Should this be expanded for environment variables?
    configsXMLName = cfg;
  }
  else
  {
    const char* re = getenv("PROJ_DIR");
    const char* pe = getenv("PROJECT");
    const char* ae = getenv("AIRCRAFT");
    const char* ie = getenv("ISFS");
    const char* ieo = getenv("ISFF");

    if (re && pe && ae)
      configsXMLName = n_u::Process::expandEnvVars(RAFXML);
    else if (ie && pe)
      configsXMLName = n_u::Process::expandEnvVars(ISFSXML);
    else if (ieo && pe)
      configsXMLName = n_u::Process::expandEnvVars(ISFFXML);
  }
  if (configsXMLName.empty())
  {
    std::ostringstream msg;
    msg << "Cannot derive path to XML project configurations.\n"
        << "Missing environment variables for $NIDAS_CONFIGS,\n"
        << RAFXML << ",\nand " << ISFSXML << "\n";
    throw NidasAppException(msg.str());
  }
  return configsXMLName;
}


NidasApp::
NidasApp(const std::string& name) :
  XmlHeaderFile
  ("-x,--xml", "<xmlfile>",
   "Specify the path to the NIDAS XML header file.\n"),
  LogShow
  ("--logshow", "",
   "As log points are created, show information for each one that can\n"
   "be used to enable log messages from that log point."),
  LogConfig
  ("-l,--log[,--logconfig,--loglevel]", "<logconfig>",
   "Add a log config to the log scheme.  The log config settings are\n"
   "specified as a comma-separated list of fields, using syntax\n"
   "<field>=<value>, where fields are tag, file, function, line, enable,\n"
   "and disable.\n"
   "The log level can be specified as either a number or string: \n"
   "7=debug,6=info,5=notice,4=warning,3=error,2=critical.\n"
   "--logconfig and --loglevel are accepted but deprecated.",
   "info"),
  LogFields
  ("--logfields", "{thread|function|file|level|time|message},...",
   "Set the log fields to be shown in log messages, as a comma-separated list\n"
   "of log field names: thread, function, file, level, time, and message."),
  LogParam
  ("--logparam", "<name>=<value>",
   "Set a log scheme parameter with syntax <name>=<value>."),
  Help
  ("-h,--help", "", "Print usage information."),
  ProcessData
  ("-p,--process", "", "Enable processed samples."),
  StartTime
  ("-s,--start", "<start-time>",
   "Start samples at start-time, in the form 'YYYY {MMM|mm} dd HH:MM[:SS]'"),
  EndTime
  ("-e,--end", "<end-time>",
   "End samples at end-time, in the form 'YYYY {MMM|mm} dd HH:MM[:SS]'"),
  SampleRanges
  ("-i,--samples", "[^]{<d1>[-<d2>|*},{<s1>[-<s2>]|*}",
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
   "              Include sample IDs 1-2 for DSMs 1-10 except for DSM 5."),
  FormatHexId("-X", "", "Format sensor-plus-sample IDs in hex"),
  FormatSampleId
  ("--id-format", "auto|decimal|hex|octal",
   "Set the output format for sensor-plus-sample IDs. The default is auto.\n"
   " auto    Use decimal for samples less than 0x8000, and hex otherwise.\n"
   " decimal Use decimal for all samples.\n"
   " hex     Use hex for all samples.\n"
   " octal   Use octal for all samples.  Not really used.",
   "auto"),
  Version
  ("-v,--version", "", "Print version information and exit."),
  InputFiles(),
  OutputFiles
  ("-o,--output", "<strptime_path>[@<number>[units]]",
   "Specify a file pattern for output files using strptime() substitutions.\n"
   "The path can optionally be followed by a file length and units:\n"
   "hours (h), minutes (m), and seconds (s). The default is seconds.\n"
   "nidas_%Y%m%d_%H%M%S.dat@30m generates files every 30 minutes."),
  Username
  ("-u,--user", "<username>",
   "Switch to the given user after setting required capabilities."),
  Hostname
  ("-H,--host", "<hostname>",
   "Run with the given hostname instead of using current system hostname."),
  DebugDaemon
  ("-d,--debug", "",
   "Run in the foreground with debug logging enabled by default, instead of\n"
   "switching to daemon mode and running in the background.  Log messages\n"
   "are written to standard error instead of syslog.  Any logging\n"
   "configuration on the command line will replace the default debug scheme."),
  ConfigsArg("-c,--configs"),
  DatasetName
  ("-S,--dataset", "<datasetname>",
   "Set environment variables specifed for the dataset\n"
   "as found in the xml file specifed by $NIDAS_DATASETS or\n"
   "$ISFS/projects/$PROJECT/ISFS/config/datasets.xml"),
  _appname(name),
  _argv0(),
  _processData(false),
  _xmlFileName(),
  _idFormat(),
  _sampleMatcher(),
  _startTime(LONG_LONG_MIN),
  _endTime(LONG_LONG_MAX),
  _dataFileNames(),
  _sockAddr(),
  _outputFileName(),
  _outputFileLength(0),
  _help(false),
  _username(),
  _hostname(),
  _userid(0),
  _groupid(0),
  _deleteProject(false),
  _app_arguments(),
  _argv(),
  _argi(0),
  _hasException(false),
  _exception(""),
  _allowUnrecognized(false)
{
  // Build configs usage here from the settings above.
  std::ostringstream configsmsg;
  configsmsg <<
    "Read the configs XML file to find the project configuration.\n"
    "These locations are checked for the XML file, depending upon\n"
    "which environment variables are set:\n"
    "   $NIDAS_CONFIGS\n"
    "   " << RAFXML << "\n"
    "   " << ISFSXML << "\n"
    "   " << ISFFXML << "\n";
  ConfigsArg.setUsageString(configsmsg.str());

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
setProcessName(const std::string& argv0)
{
  _argv0 = argv0;
}

std::string
NidasApp::
getProcessName()
{
  if (_argv0.empty())
  {
    return getName();
  }
  return _argv0;
}

ArgVector
NidasApp::
parseArgs(int argc, const char* const argv[]) throw (NidasAppException)
{
  if (_argv0.empty())
  {
    setProcessName(argv[0]);
  }
  return parseArgs(ArgVector(argv+1, argv+argc));
}

void
NidasApp::
enableArguments(const nidas_app_arglist_t& arglist)
{
  _app_arguments = _app_arguments | arglist;
}


void
NidasApp::
requireArguments(const nidas_app_arglist_t& arglist)
{
  // All required arguments are implicitly enabled.
  enableArguments(arglist);
  nidas_app_arglist_t::const_iterator it;
  for (it = arglist.begin(); it != arglist.end(); ++it) {
    (*it)->setRequired();
  }
}


void
NidasApp::
checkRequiredArguments()
{
  nidas_app_arglist_t args = getArguments();
  nidas_app_arglist_t::const_iterator it = args.begin();
  for (it = args.begin(); it != args.end(); ++it)
  {
    if ((*it)->isRequired() && !((*it)->specified()))
    {
      std::ostringstream msg;
      msg << "Missing required argument: " << (*it)->getUsageFlags();
      DLOG(("") << msg.str());
      throw NidasAppException(msg.str());
    }
  }
}


nidas_app_arglist_t
NidasApp::
getArguments()
{
  return nidas_app_arglist_t(_app_arguments.begin(), _app_arguments.end());
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


ArgVector
NidasApp::
unparsedArgs()
{
  return _argv;
}


void
NidasApp::
startArgs(const ArgVector& args)
{
  _argv = args;
  _argi = 0;
}


void
NidasApp::
startArgs(int argc, const char* const argv[]) throw (NidasAppException)
{
  if (_argv0.empty())
  {
    setProcessName(argv[0]);
  }
  startArgs(ArgVector(argv+1, argv+argc));
}


bool
NidasApp::
nextArg(std::string& arg)
{
  bool found = false;
  if (_argi < (int)_argv.size() && _argv[_argi].find('-') != 0)
  {
    found = true;
    arg = _argv[_argi];
    _argv.erase(_argv.begin() + _argi, _argv.begin() + _argi + 1);
  }
  return found;
}


NidasAppArg*
NidasApp::
parseNext() throw (NidasAppException)
{
  NidasAppArg* arg = 0;
  while (!arg && _argi < (int)_argv.size())
  {
    nidas_app_arglist_t::iterator it;
    int i = _argi;
    for (it = _app_arguments.begin(); it != _app_arguments.end(); ++it)
    {
      if ((*it)->parse(_argv, &i))
      {
	arg = *it;
	// Remove arguments [istart, i], and leave _argi pointing at the
	// next argument which moves into that spot.
	_argv.erase(_argv.begin() + _argi, _argv.begin() + i + 1);
	break;
      }
    }
    if (!arg && _argv[i].substr(0, 1) == "-" && !allowUnrecognized())
    {
      throw NidasAppException("Unrecognized argument: " + _argv[i]);
    }
    if (!arg)
    {
      ++_argi;
    }
  }
  if (arg == &XmlHeaderFile)
  {
    _xmlFileName = XmlHeaderFile.getValue();
  }
  else if (arg == &SampleRanges)
  {
    std::string optarg = SampleRanges.getValue();
    if (! _sampleMatcher.addCriteria(optarg))
    {
      throw NidasAppException("sample criteria could not be parsed: " +
			      optarg);
    }
    if (optarg.find("0x", 0) != string::npos && _idFormat._idFormat == NOFORMAT_ID)
    {
      setIdFormat(HEX_ID);
    }
  }
  else if (arg == &FormatHexId)
  {
    setIdFormat(HEX_ID);
  }
  else if (arg == &FormatSampleId)
  {
    std::string optarg = FormatSampleId.getValue();
    if (optarg == "auto")
      setIdFormat(AUTO_ID);
    else if (optarg == "decimal")
      setIdFormat(DECIMAL_ID);
    else if (optarg == "hex")
      setIdFormat(HEX_ID);
    else if (optarg == "octal")
      setIdFormat(OCTAL_ID);
    else
    {
      std::ostringstream msg;
      msg << "Wrong format '" << optarg << "'. "
	  << "Sample ID format must be auto, decimal, hex, or octal.";
      throw NidasAppException(msg.str());
    }
  }
  else if (arg == &LogConfig)
  {
    parseLogConfig(LogConfig.getValue());
  }
  else if (arg == &LogFields)
  {
    Logger* logger = Logger::getInstance();
    LogScheme scheme = logger->getScheme(getName());
    scheme.setShowFields(LogFields.getValue());
    logger->setScheme(scheme);
  }
  else if (arg == &LogParam)
  {
    Logger* logger = Logger::getInstance();
    LogScheme scheme = logger->getScheme(getName());
    scheme.parseParameter(LogParam.getValue());
    logger->setScheme(scheme);
  }
  else if (arg == &LogShow)
  {
    Logger* logger = Logger::getInstance();
    LogScheme scheme = logger->getScheme(getName());
    scheme.showLogPoints(true);
    logger->setScheme(scheme);
  }
  else if (arg == &ProcessData)
  {
    _processData = true;
  }
  else if (arg == &EndTime)
  {
    _endTime = parseTime(EndTime.getValue());
    _sampleMatcher.setEndTime(_endTime);
  }
  else if (arg == &StartTime)
  {
    _startTime = parseTime(StartTime.getValue());
    _sampleMatcher.setStartTime(_startTime);
  }
  else if (arg == &OutputFiles)
  {
    parseOutput(OutputFiles.getValue());
  }
  else if (arg == &Version)
  {
    std::cout << "Version: " << Version::getSoftwareVersion() << std::endl;
    exit(0);
  }
  else if (arg == &Hostname)
  {
    _hostname = Hostname.getValue();
  }
  else if (arg == &Username)
  {
    parseUsername(Username.getValue());
  }
  else if (arg == &Help)
  {
    _help = true;
  }
  return arg;
}


ArgVector
NidasApp::
parseArgs(const ArgVector& args) throw (NidasAppException)
{
  startArgs(args);
  NidasAppArg* arg = parseNext();
  while (arg)
  {
    arg = parseNext();
  }
  return unparsedArgs();
}


void
NidasApp::
parseInputs(const std::vector<std::string>& inputs_,
	    std::string default_input,
	    int default_port) throw (NidasAppException)
{
  std::vector<std::string> inputs(inputs_);
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
    {
      _dataFileNames.push_back(url);
    }
  }
  static n_u::LogContext lp(LOG_DEBUG);
  if (lp.active())
  {
    n_u::LogMessage msg(&lp);
    msg << "parseInputs() found " << _dataFileNames.size() << " input files; ";
    if (_sockAddr.get())
    {
      msg << "and socket input " << _sockAddr->toAddressString();
    }
    else
    {
      msg << "and no socket input set.";
    }
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
  }
  // If no output length suffix was specified, then the output length is
  // reset to zero.  This way the current settings match with the most
  // recent output option on the command line, rather than possibly
  // "inheriting" an output length specified with a different output file
  // name.
  _outputFileLength = length;
  _outputFileName = output;
}


namespace
{
  void (*app_interrupted_callback)(int) = 0;

  bool app_interrupted = false;

  sigset_t logmask;
  bool logmask_cleared = false;

  void sigAction(int sig, siginfo_t* siginfo, void*)
  {
    if (!sigismember(&logmask, sig))
    {
      std::cerr <<
	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << std::endl;
    }

    // There used to be a switch statement which selected on the signal
    // number being one of the ones that were added to the handler, but
    // that should not be necessary, since this handler will only be called
    // if it's one of the signals it's supposed to handle.
    app_interrupted = true;
    if (app_interrupted_callback)
      (*app_interrupted_callback)(sig);
  }

}


bool
NidasApp::
interrupted(bool allow_exception)
{
  if (allow_exception && hasException())
  {
    throw getException();
  }
  return app_interrupted;
}


void
NidasApp::
setException(const nidas::util::Exception& ex)
{
  _hasException = true;
  _exception = ex;
  setInterrupted(true);
}


bool
NidasApp::
hasException()
{
  return _hasException;
}


nidas::util::Exception
NidasApp::
getException()
{
  return _exception;
}


void
NidasApp::
setInterrupted(bool interrupted)
{
  app_interrupted = interrupted;
}


/* static */
void
NidasApp::
setupSignals(void (*callback)(int signum))
{
  addSignal(SIGHUP, callback);
  addSignal(SIGTERM, callback);
  addSignal(SIGINT, callback);
}


/* static */
void
NidasApp::
addSignal(int signum, void (*callback)(int signum), bool nolog)
{
  if (!logmask_cleared)
  {
    sigemptyset(&logmask);
    logmask_cleared = true;
  }
  if (nolog)
  {
    sigaddset(&logmask, signum);
  }

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signum);
  sigprocmask(SIG_UNBLOCK, &sigset, (sigset_t*)0);

  struct sigaction act;
  sigemptyset(&sigset);
  act.sa_mask = sigset;
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = sigAction;
  sigaction(signum, &act, (struct sigaction *)0);
  app_interrupted_callback = callback;
}


nidas_app_arglist_t
NidasApp::
loggingArgs()
{
  nidas_app_arglist_t args = 
    LogConfig | LogShow | LogFields | LogParam;
  return args;
}


nidas_app_arglist_t
nidas::core::
operator|(nidas_app_arglist_t arglist1, nidas_app_arglist_t arglist2)
{
  nidas_app_arglist_t::iterator it;
  nidas_app_arglist_t::iterator lookup;
  for (it = arglist2.begin(); it != arglist2.end(); ++it)
  {
    lookup = std::find(arglist1.begin(), arglist1.end(), *it);
    if (lookup == arglist1.end())
      arglist1.push_back(*it);
  }
  return arglist1;
}


std::string
NidasApp::
usage(const std::string& indent)
{
  // Iterate through the list this application's arguments, dumping usage
  // info for each.
  std::ostringstream oss;
  nidas_app_arglist_t::iterator it;
  for (it = _app_arguments.begin(); it != _app_arguments.end(); ++it)
  {
    NidasAppArg& arg = (**it);
    oss << arg.usage(indent);
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
  if (default_input.length())
    oss << "Default inputURL is \"" << default_input << "\"\n";
  setUsageString(oss.str());
}

void
NidasApp::
setIdFormat(IdFormat idt)
{
  _idFormat = idt;
}


std::ostream&
NidasApp::
formatSampleId(std::ostream& leader, IdFormat idFormat, dsm_sample_id_t sampid)
{
  // int dsmid = GET_DSM_ID(sampid);
  int spsid = GET_SHORT_ID(sampid);
  id_format_t spsfmt = idFormat.idFormat();
  int width = idFormat.decimalWidth();

  if (spsfmt == AUTO_ID && spsid >= 0x8000)
    spsfmt = HEX_ID;
  else if (spsfmt == AUTO_ID)
    spsfmt = DECIMAL_ID;

  // leader << setw(2) << setfill(' ') << dsmid << ',';
  switch(spsfmt) {
  case NidasApp::HEX_ID:
    leader << "0x" << setw(4) << setfill('0') << hex << spsid
	   << setfill(' ') << dec << ' ';
    break;
  case NidasApp::OCTAL_ID:
    leader << "0" << setw(6) << setfill('0') << oct << spsid
	   << setfill(' ') << dec << ' ';
    break;
  default:
    leader << setw(width) << spsid << ' ';
    break;
  }
  return leader;
}


std::ostream&
NidasApp::
formatSampleId(std::ostream& out, dsm_sample_id_t spsid)
{
  return NidasApp::formatSampleId(out, getIdFormat(), spsid);
}


std::string
NidasApp::
formatId(dsm_sample_id_t sid)
{
  std::ostringstream out;
  out << GET_DSM_ID(sid) << ",";
  formatSampleId(out, sid);
  return out.str();
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
allowUnrecognized(bool allow)
{
  _allowUnrecognized = allow;
}


bool
NidasApp::
allowUnrecognized()
{
  return _allowUnrecognized;
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


void
NidasApp::
setupDaemon()
{
  nidas::util::Logger* logger = 0;
  n_u::LogConfig lc;
  n_u::LogScheme logscheme(getName());
  if (! DebugDaemon.asBool())
  {
    lc.level = n_u::LOGGER_DEBUG;
    // fork to background, chdir to /, send stdout/stderr to /dev/null
    if (daemon(0,0) < 0)
    {
      n_u::IOException e(getProcessName(), "daemon", errno);
      cerr << "Warning: " << e.toString() << endl;
    }
    logger = n_u::Logger::createInstance(getName().c_str(), LOG_PID, LOG_LOCAL5);
    logscheme.setShowFields("level,message");
  }
  else
  {
    lc.level = n_u::LOGGER_INFO;
    logger = n_u::Logger::createInstance(&std::cerr);
  }
  logscheme.addConfig(lc);
  logger->setScheme(logscheme);
}



void
NidasApp::
lockMemory()
{
#ifdef DO_MLOCKALL
  try {
    n_u::Process::addEffectiveCapability(CAP_IPC_LOCK);
  }
  catch (const n_u::Exception& e) {
    WLOG(("%s: %s. Cannot add CAP_IPC_LOCK capability, "
	  "memory locking is not possible", _argv0, e.what()));
  }
  ILOG(("Locking memory: mlockall(MCL_CURRENT | MCL_FUTURE)"));
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
    n_u::IOException e(_argv0, "mlockall", errno);
    WLOG(("%s", e.what()));
  }
#else
  DLOG(("Locking memory: not compiled."));
#endif
}



void
NidasApp::
setupProcess()
{
#ifdef HAVE_SYS_CAPABILITY_H 
  /* man 7 capabilities:
   * If a thread that has a 0 value for one or more of its user IDs wants to
   * prevent its permitted capability set being cleared when it  resets  all
   * of  its  user  IDs  to  non-zero values, it can do so using the prctl()
   * PR_SET_KEEPCAPS operation.
   *
   * If we are started as uid=0 from sudo, and then setuid(x) below
   * we want to keep our permitted capabilities.
   */
  try {
    if (prctl(PR_SET_KEEPCAPS,1,0,0,0) < 0)
      throw n_u::Exception("prctl(PR_SET_KEEPCAPS,1)",errno);
  }
  catch (const n_u::Exception& e) {
    WLOG(("") << _argv0 << ": "
	 << e.what() << ". Will not be able to use real-time priority");
  }
#endif

  gid_t gid = getGroupID();
  if (gid != 0 && getegid() != gid)
  {
    DLOG(("doing setgid(%d)", gid));
    if (setgid(gid) < 0)
    {
      WLOG(("") << _argv0 << ": cannot change group id to " << gid
	   << ": " << strerror(errno));
    }
  }

  uid_t uid = getUserID();
  if (uid != 0 && geteuid() != uid)
  {
    DLOG(("doing setuid(%d=%s)", uid, getUserName().c_str()));
    if (setuid(uid) < 0)
      WLOG(("") << _argv0 << ": cannot change userid to " << uid
	   << " (" << getUserName() << "): " << strerror(errno));
  }

#ifdef CAP_SYS_NICE
  try {
    n_u::Process::addEffectiveCapability(CAP_SYS_NICE);
#ifdef DEBUG
    DLOG(("CAP_SYS_NICE = ")
	 << n_u::Process::getEffectiveCapability(CAP_SYS_NICE));
    DLOG(("PR_GET_SECUREBITS=")
	 << hex << prctl(PR_GET_SECUREBITS,0,0,0,0) << dec);
#endif
  }
  catch (const n_u::Exception& e)
  {
    WLOG(("") << _argv0 << ": " << e.what());
  }

  if (!n_u::Process::getEffectiveCapability(CAP_SYS_NICE))
  {
    WLOG(("") << _argv0 << ": CAP_SYS_NICE not in effect. "
	 "Will not be able to use real-time priority");
  }
  try
  {
    n_u::Process::addEffectiveCapability(CAP_NET_ADMIN);
  }
  catch (const n_u::Exception& e)
  {
    WLOG(("") << _argv0 << ": " << e.what());
  }
#endif
}



void
NidasApp::
parseUsername(const std::string& username)
{
  struct passwd pwdbuf;
  struct passwd *result;
  long nb = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (nb < 0) nb = 4096;
  vector<char> strbuf(nb);
  int res;
  if ((res = getpwnam_r(username.c_str(), &pwdbuf,
			&strbuf.front(), nb, &result)) != 0)
  {
    ostringstream msg;
    msg << "getpwnam_r: " << n_u::Exception::errnoToString(res);
    throw NidasAppException(msg.str());
  }
  else if (result == 0)
  {
    ostringstream msg;
    msg << "Unknown user: " << username;
    throw NidasAppException(msg.str());
  }
  _username = username;
  _userid = pwdbuf.pw_uid;
  _groupid = pwdbuf.pw_gid;
}


namespace
{
  const char* ISFSDATASETSXML = "$ISFS/projects/$PROJECT/ISFS/config/datasets.xml";
  const char* ISFFDATASETSXML = "$ISFF/projects/$PROJECT/ISFF/config/datasets.xml";
}


Dataset
NidasApp::
getDataset(const std::string& datasetname)
  throw(n_u::InvalidParameterException, XMLException)
{
  string XMLName;
  const char* ndptr = getenv("NIDAS_DATASETS");

  if (ndptr)
  {
    XMLName = string(ndptr);
    ILOG(("") << "Using dataset XML path from NIDAS_DATASETS: " << XMLName);
  }
  else
  {
    const char* ie = ::getenv("ISFS");
    const char* ieo = ::getenv("ISFF");
    const char* pe = ::getenv("PROJECT");
    if (ie && pe)
    {
      XMLName = n_u::Process::expandEnvVars(ISFSDATASETSXML);
      ILOG(("") << "Using dataset XML path from ISFS: " << XMLName);
    }
    else if (ieo && pe)
    {
      XMLName = n_u::Process::expandEnvVars(ISFFDATASETSXML);
      ILOG(("") << "Using dataset XML path from ISFF: " << XMLName);
    }
  }
  if (XMLName.length() == 0)
  {
    std::ostringstream msg;
    msg << "Datasets XML path could not be derived from one of these paths:\n"
        << " $NIDAS_DATASETS\n"
        << " " << ISFSDATASETSXML << "\n"
        << " " << ISFFDATASETSXML << "\n";
    WLOG(("") << msg.str());
    throw NidasAppException(msg.str());
  }

  Datasets datasets;
  datasets.parseXML(XMLName);

  Dataset dataset = datasets.getDataset(datasetname);
  dataset.putenv();
  return dataset;
}



std::string
NidasApp::
getHostName()
{
  if (_hostname.empty())
  {
    char hostnamechr[256];
    size_t hlen = sizeof(hostnamechr);
    if (::gethostname(hostnamechr, hlen) < 0)
    {
      if (errno == ENAMETOOLONG)
      {
        hostnamechr[hlen-1] = 0;
      }
      else
      {
        string estring(strerror(errno));
        ELOG(("gethostname: ") << estring);
        hostnamechr[0] = 0;
      }
    }
    _hostname = hostnamechr;
  }
  return _hostname;
}


std::string
NidasApp::
getShortHostName()
{
  string hostname = getHostName();
  return hostname.substr(0, hostname.find('.'));
}


int
NidasApp::
checkPidFile()
{
  // Open and check the pid file after the above setuid() and daemon() calls.
  if (! DebugDaemon.asBool())
  {
    try
    {
      string pidname = "/tmp/run/nidas";
      mode_t mask = ::umask(0);
      n_u::FileSet::createDirectory(pidname, 01777);

      pidname += "/";
      pidname += getName() + ".pid";
      pid_t pid = n_u::Process::checkPidFile(pidname);
      ::umask(mask);

      if (pid > 0)
      {
	PLOG(("") << getProcessName() << ": pid=" << pid
	     << " is already running");
	return 1;
      }
    }
    catch(const n_u::IOException& e)
    {
      PLOG(("") << getProcessName() << ": " << e.what());
      return 1;
    }
  }
  return 0;
}
