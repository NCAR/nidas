
#include "NidasApp.h"

#include <nidas/core/Version.h>

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sstream>
#include <stdexcept>

using std::string;

using namespace nidas::core;
namespace n_u = nidas::util;

SampleMatcher::
RangeMatcher::
RangeMatcher(int d1, int d2, int s1, int s2, int inc) :
  dsm1(d1), dsm2(d2), sid1(s1), sid2(s2), include(inc)
{}


SampleMatcher::
SampleMatcher() :
  _ranges(),
  _lookup()
{
}


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


  void
  parse_range(const std::string& rngstr, int& rngid1, int& rngid2)
  {
    string::size_type ic;
    if (rngstr.length() > 1 && (ic = rngstr.find('-',1)) != string::npos)
    {
      rngid1 = int_from_string(rngstr.substr(0,ic));
      rngid2 = int_from_string(rngstr.substr(ic+1));
    }
    else if (rngstr == "*")
    {
      rngid1 = rngid2 = -1;
    }
    else
    {
      rngid1 = rngid2 = int_from_string(rngstr);
    }
  }
}


bool
SampleMatcher::
addCriteria(const std::string& ctext)
{
  bool include = true;
  int dsmid1,dsmid2;
  int snsid1,snsid2;
  string soptarg(ctext);
  if (soptarg.empty())
    return false;
  if (soptarg[0] == '^')
  {
    include = false;
    soptarg = soptarg.substr(1);
  }
  string::size_type ic = soptarg.find(',');
  if (ic == string::npos) 
    return false;
  string dsmstr = soptarg.substr(0,ic);
  string snsstr = soptarg.substr(ic+1);
  try
  {
    parse_range(dsmstr, dsmid1, dsmid2);
    parse_range(snsstr, snsid1, snsid2);
  }
  catch (std::invalid_argument& err)
  {
    return false;
  }
#ifdef notdef
  if (snsstr.find("0x",0) != string::npos) 
    _idFormat = DumpClient::HEX_ID;
#endif

  _lookup.clear();
  _ranges.push_back(RangeMatcher(dsmid1, dsmid2, snsid1, snsid2, include));
  return true;
}

bool
SampleMatcher::
match(dsm_sample_id_t id)
{
  id_lookup_t::iterator it = _lookup.find(id);
  if (it != _lookup.end())
  {
    return it->second;
  }
  bool all_excludes = true;
  range_matches_t::iterator ri;
  for (ri = _ranges.begin(); ri != _ranges.end(); ++ri)
  {
    all_excludes = all_excludes && (!ri->include);
    // See if the id is in this range, in which case the result depends
    // upon whether this range is included or excluded.
    int did = GET_DSM_ID(id);
    int sid = GET_SHORT_ID(id);
    if ((ri->dsm1 == -1 || (did >= ri->dsm1 && did <= ri->dsm2)) && 
	(ri->sid1 == -1 || (sid >= ri->sid1 && sid <= ri->sid2)))
    {
      break;
    }
  }
  // If there are no ranges, then the sample is implicitly included.  If
  // there are only excluded ranges and this sample did not match any of
  // them, then the sample is implicitly included.
  bool result = false;
  if (_ranges.begin() == _ranges.end())
    result = true;
  else if (ri != _ranges.end())
    result = ri->include;
  else if (all_excludes)
    result = true;
  _lookup[id] = result;
  return result;
}


bool
SampleMatcher::
exclusiveMatch()
{
  bool xc = false;
  if (_ranges.size() == 1)
  {
    range_matches_t::iterator ri = _ranges.begin();
    xc = ((ri->dsm1 == ri->dsm2 && ri->dsm1 != -1) &&
	  (ri->sid1 == ri->sid2 && ri->sid1 != -1));
  }
  return xc;
}


NidasApp::
NidasApp(const std::string& name) :
  XmlHeaderFile
  ("-x", "--xml",
   "Path to the NIDAS XML header file.  The default path is\n"
   "taken from the header and expanded "
   "using the current environment settings.",
   "<xmlfile>"),
  LogLevel
  ("-l", "--loglevel",
   "Specify the log level as either a number or string: \n"
   "7=debug,6=info,5=notice,4=warning,3=error,2=critical. Default is info.",
   "<loglevel>"),
  Help
  ("-h", "--help", "Print usage information."),
  ProcessData
  ("-p", "--process", "Enable processed samples."),
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
   "Sample ids can be specified in 0x hex format with a leading 0x.\n"
   "Prefix the range option with ^ to exclude that range of samples.\n"
   "Multiple range options can be specified.  Samples are either included\n"
   "or excluded according to the first option which matches their ID.\n"
   "If only exclusions are specified, then all other samples are implicitly\n"
   "included.\n"
   "Example: \n"
   " -i ^1,-1     Include all samples except those with DSM ID 1.\n"
   " -i ^5,* --samples 1-10,1-2\n"
   "              Include sample IDs 1-2 for DSMs 1-10 except for DSM 5.\n"
   "Use data_stats program to see DSM ids and sample ids of data in a file.",
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
  _logLevel(n_u::LOGGER_INFO),
  _processData(false),
  _xmlFileName(),
  _idFormat(DECIMAL),
  _sampleMatcher(),
  _startTime(LONG_LONG_MIN),
  _endTime(LONG_LONG_MAX),
  _dataFileNames(),
  _sockAddr(0),
  _outputFileName(),
  _outputFileLength(0),
  _deleteProject(false)
{
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
parseLogLevel(const std::string& optarg) throw (NidasAppException)
{
  // log level can be a number 1-7 or the name of a log level.
  int level = atoi(optarg.c_str());
  if (level < 1 || level > 7)
    level = nidas::util::stringToLogLevel(optarg);
  if (level < 0)
  {
    throw NidasAppException("unknown log level: " + string(optarg));
  }
  {
    _logLevel = level;
    nidas::util::LogConfig lc;
    lc.level = _logLevel;
    nidas::util::Logger::getInstance()->setScheme
      (nidas::util::LogScheme(getName()).addConfig (lc));
  }
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
    }
    else if (LogLevel.accept(arg))
    {
      parseLogLevel(xarg(args, ++i));
    }
    else if (ProcessData.accept(arg))
    {
      _processData = true;
    }
    else if (EndTime.accept(arg))
    {
      _endTime = parseTime(xarg(args, ++i));
    }
    else if (StartTime.accept(arg))
    {
      _startTime = parseTime(xarg(args, ++i));
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
	    const std::string& default_input,
	    int default_port) throw (NidasAppException)
{
  if (inputs.size() == 0 && default_input.length() > 0)
    inputs.push_back(default_input);

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

#ifdef notdef
  if (_dataFileNames.size() == 0 && !_sockAddr.get()) return usage(argv[0]);
#endif
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
    XmlHeaderFile | LogLevel | Help | ProcessData | StartTime |
    EndTime | SampleRanges | Version | InputFiles | OutputFiles;

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
      oss << arg.usage() << "\n";
      oss << "\n";
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

