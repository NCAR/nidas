// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
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

#include "Logger.h"
#include "ThreadSupport.h"
#include "Thread.h"
#include "UTime.h"

using nidas::util::Mutex;
using nidas::util::Synchronized;
using nidas::util::Thread;

#include <string>
#include <cerrno>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

using namespace nidas::util;
using namespace std;

//================================================================
// LoggerPrivate
//================================================================

typedef vector<LogContext*> log_points_v;
typedef map<string,LogScheme> log_schemes_t;

static Mutex logger_mutex;

static log_points_v log_points;
static log_schemes_t log_schemes;
static LogScheme current_scheme;

using std::cerr;

#include <execinfo.h>

static void
stream_backtrace (std::ostream& out)
{
  void *array[200];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, 200);
  strings = backtrace_symbols (array, size);

  out << "Obtained " << size << " stack frames:\n";
  for (i = 0; i < size; i++)
    out << "  " << i << ". " << strings[i] << "\n";

  free (strings);
}

#ifndef DEBUG_LOGGER
#define DEBUG_LOGGER 0
#endif

namespace nidas { namespace util { 

struct LoggerPrivate
{
  static void
  reconfig (log_points_v::iterator firstpoint)
  {
    if (DEBUG_LOGGER)
    {
      stream_backtrace (cerr);
      cerr << "current scheme: " << current_scheme.getName() << "\n";
      cerr << " - " << current_scheme.log_configs.size() << " configs\n"
	   << " - showfields: " 
	   << current_scheme.getShowFieldsString() << "\n";
      cerr << "currently " << log_points.size() << " log points.\n";
    }
    LogScheme::log_configs_v& log_configs = current_scheme.log_configs;
    LogScheme::log_configs_v::iterator firstconfig = log_configs.begin();
    LogScheme::log_configs_v::iterator ic;
    log_points_v::iterator it;
    for (it = firstpoint ; it != log_points.end(); ++it)
    {
      // If there are no configs, then disable the log point.
      if (firstconfig == log_configs.end())
	(*it)->_active = false;
      for (ic = firstconfig ; ic != log_configs.end(); ++ic)
      {
	if (ic->matches(*(*it)))
	{
	  if (DEBUG_LOGGER)
	  {
	    cerr << "reconfig: =="
		 << (ic->activate ? " on" : "off") << "==> "
		 << (*it)->_file << ":" << (*it)->_line
		 << ":" << (*it)->_function << endl;
	  }
	  (*it)->_active = ic->activate;
	}
      }
    }
  }
};

}} // nidas::util
  

//================================================================
// Logger
//================================================================

Logger::Logger(const char *ident, int logopt, int facility, const char *TZ):
	output(0),syslogit(true),loggerTZ(0),saveTZ(0) {
  // open syslog connection
  ::openlog(ident,logopt,facility);
  setTZ(TZ);
}

Logger::Logger(ostream* out) : 
  output(out),syslogit(false),loggerTZ(0),saveTZ(0) 
{
}

Logger::Logger() : output(&cerr),syslogit(false),loggerTZ(0),saveTZ(0) 
{
}

Logger::~Logger() {
  if (syslogit) ::closelog();
  if (output) output->flush();
  delete [] loggerTZ;
  delete [] saveTZ;
  _instance = 0;
}

/* static */
Logger* Logger::_instance = 0;

/* static */
Logger* 
Logger::
createInstance(const char *ident, int logopt, int facility, const char *TZ) 
{
  Synchronized sync(logger_mutex);
  delete _instance;
  _instance = new Logger(ident,logopt,facility,TZ);
  return _instance;
}

/* static */
Logger* Logger::createInstance(ostream* out) 
{
  Synchronized sync(logger_mutex);
  delete _instance;
  _instance = new Logger(out);
  return _instance;
}

/* static */
Logger* Logger::getInstance() {
  if (!_instance) createInstance(&cerr);
  return _instance;
}

void Logger::setTZ(const char* val) {

  delete [] loggerTZ;
  loggerTZ = 0;

  delete [] saveTZ;
  saveTZ = 0;

  if (!val) return;	// user wants default TZ

  const char *tz;
  if (!(tz = getenv("TZ"))) tz = "GMT";

  // it is risky to use string.c_str() for these TZ
  // environment varibles. The pointer must always be valid
  // so we use simple char*.
  if (strcmp(val,tz)) {
    loggerTZ = new char[strlen(val) + 4];
    strcpy(loggerTZ,"TZ=");
    strcat(loggerTZ,val);

    saveTZ = new char[strlen(tz) + 4];
    strcpy(saveTZ,"TZ=");
    strcat(saveTZ,tz);
  }
}


void
Logger::
msg (const LogContext& lc, const string& msg)
{
  static const char* fixedsep = "|";
  const char* sep = "";
  Synchronized sync(logger_mutex);

  if (loggerTZ) {
    putenv(loggerTZ);
    tzset();
  }
  ostringstream oss;

  for (unsigned int i = 0; i < current_scheme.log_fields.size(); ++i)
  {
    LogScheme::LogField show = current_scheme.log_fields[i];

    if (show & LogScheme::FileField)
    {
      oss << sep << lc.filename() << "(" << lc.line() << ")" ;
      sep = fixedsep;
    }
    if (show & LogScheme::LevelField)
    {
      string level = lc.levelName();
      for (unsigned int i = 0; i < level.length(); ++i)
	level[i] = toupper(level[i]);
      oss << sep << level;
      sep = fixedsep;
    }
    if (show & LogScheme::FunctionField)
    {
      oss << sep << lc.function();
      sep = fixedsep;
    }
    if (show & LogScheme::ThreadField)
    {
      oss << sep << "~" << Thread::currentName() << "~";
      sep = fixedsep;
    }
    if (show & LogScheme::TimeField)
    {
      UTime now;
      oss << sep << now.setFormat("%F,%T");
      sep = fixedsep;
    }
    if (show & LogScheme::MessageField)
    {
      oss << sep << msg;
      sep = fixedsep;
    }
  }
  if (oss.str().length() > 0)
  {
    if (syslogit)
    {
      syslog(lc.level(), "%s", oss.str().c_str());
    }
    else
    {
      *output << oss.str() << "\n";
    }
  }
  if (loggerTZ) {
    putenv(saveTZ);
    tzset();
  }
}
  

/*
 * Replace %m with strerror(errno), like syslog does.
 */
static string
fillError(string fmt)
{
    int err = errno;	// grab it in case it changes!

    string::size_type mp;
    while ((mp = fmt.find ("%m")) != string::npos)
    {
      fmt.replace (mp, 2, strerror(err));
    }
    return fmt;
}


#if defined(SVR4) || ( defined(__GNUC__) && __GNUC__ > 1 )
void Logger::log(int severity, const char* file, const char* fn,
		 int line, const char *fmt, ...)
#elif defined(__GNUC__)
void Logger::log(...)
#else
void Logger::log(va_alist)
va_dcl
#endif
{
  va_list args;
#if !defined(SVR4) && ( !defined(__GNUC__) || __GNUC__ < 2 )
  int severity;
  const char* file;
  const char* fn;
  int line;
  const char *fmt;
#endif

#if defined(SVR4) || ( defined(__GNUC__) && __GNUC__ > 1 )
  va_start(args,fmt);
#else
  va_start(args);
  severity = va_arg(args,int);
  file = va_arg(args,const char*);
  fn = va_arg(args,const char*);
  line = va_arg(args,int);
  fmt = va_arg(args,const char *);
#endif

  // Construct a LogContext here, then check if it's enabled by
  // the current configuration.  It will be destroyed when this
  // function returns.
  LogContext lc(severity, file, fn, line);
  if (lc.active())
  {
    string newfmt = fillError(fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), newfmt.c_str(), args);
    msg (lc, buffer);
  }
  va_end(args);
}


void
Logger::
setScheme(const string& name)
{
  if (name.length() > 0)
  {
    Synchronized sync(logger_mutex);
    // This might actually create a new scheme, and so we have
    // to make sure we set the name on it.
    log_schemes[name].setName(name);
    current_scheme = log_schemes[name];
    LoggerPrivate::reconfig(log_points.begin());
  }
}


void
Logger::
setScheme(const LogScheme& scheme)
{
  Synchronized sync(logger_mutex);
  log_schemes[scheme.getName()] = scheme;
  current_scheme = scheme;
  LoggerPrivate::reconfig(log_points.begin());
}


LogScheme
Logger::
getScheme(const std::string& name) const
{
  Synchronized sync(logger_mutex);
  if (name.length() == 0)
    return current_scheme;
  log_schemes[name].setName(name);
  return log_schemes[name];
}


void
Logger::
updateScheme(const LogScheme& scheme)
{
  Synchronized sync(logger_mutex);
  log_schemes[scheme.getName()] = scheme;
  if (current_scheme.getName() == scheme.getName())
  {
    current_scheme = scheme;
    LoggerPrivate::reconfig(log_points.begin());
  }
}
  

//================================================================
// LogMessage
//================================================================

LogMessage&
LogMessage::
format(const char *fmt, ...)
{
  va_list args;
  va_start(args,fmt);

  string newfmt = fillError(fmt);
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), newfmt.c_str(), args);
  va_end(args);
  this->msg += buffer;
  return *this;
}


//================================================================
// LogScheme
//================================================================

// Initialize the level and field string maps in the constructor,
// to avoid the need to synchronize initialization later.

namespace {
    typedef map<string, LogScheme::LogField> show_strings_t;

    // class with constructor for initializing show_strings_t.
    struct show_strings_initializer
    {
        show_strings_initializer(): _map()
        {
            _map["thread"] = LogScheme::ThreadField;
            _map["function"] = LogScheme::FunctionField;
            _map["file"] = LogScheme::FileField;
            _map["level"] = LogScheme::LevelField;
            _map["time"] = LogScheme::TimeField;
            _map["message"] = LogScheme::MessageField;
            _map["all"] = LogScheme::AllFields;
            _map["none"] = LogScheme::NoneField;
        }
        show_strings_t _map;
    } _show_strings;

    show_strings_t& show_strings = _show_strings._map;
}

LogScheme::
LogScheme(const std::string& name) :
  _name (name),log_configs(),log_fields()
{
  // Force an empty name to the default, non-empty name.
  if (name.length() == 0)
  {
    *this = LogScheme();
  }
  else
  {
    log_fields.push_back(TimeField);
    log_fields.push_back(LevelField);
    log_fields.push_back(MessageField);

    // Default to logging everything warning and above.
    LogConfig lc;
    lc.level = LOGGER_NOTICE;
    addConfig (lc);
  }
}


LogScheme&
LogScheme::
setShowFields(const std::vector<LogField>& fields)
{
  log_fields = fields;
  return *this;
}


LogScheme&
LogScheme::
setShowFields(const std::string& fields)
{
  // Parse the string for field names and construct a vector from them.
  std::vector<LogField> fv;
  string::size_type at = 0;
  do {
    string::size_type comma = fields.find(',', at);
    if (comma == string::npos) 
      comma = fields.length();
    LogField f = stringToField(fields.substr(at, comma-at));
    if (f)
    {
      fv.push_back(f);
    }
    at = comma+1;
  }
  while (at < fields.length());
  return setShowFields (fv);
}


std::string
LogScheme::
getShowFieldsString () const
{
  std::ostringstream oss;
  for (unsigned int i = 0; i < log_fields.size(); ++i)
  {
    if (i > 0) oss << ",";
    oss << fieldToString(log_fields[i]);
  }
  return oss.str();
}


LogScheme&
LogScheme::
clearConfigs()
{
  log_configs.clear();
  return *this;
}


LogScheme&
LogScheme::
addConfig(const LogConfig& lc)
{
  log_configs.push_back(lc);
  return *this;
}


LogScheme::LogField
LogScheme::
stringToField(const std::string& sin)
{
  //  init_level_map();
  string s = sin; 
  for (unsigned int i = 0; i < s.length(); ++i)
    s[i] = tolower(s[i]);
  show_strings_t::iterator it = show_strings.find(s);
  if (it != show_strings.end())
    return it->second;
  return NoneField;
}


std::string
LogScheme::
fieldToString(LogScheme::LogField lf)
{
  show_strings_t::iterator it;
  for (it = show_strings.begin(); it != show_strings.end(); ++it)
  {
    if (it->second == lf)
      return it->first;
  }
  return "none";
}

//================================================================
// LogContext
//================================================================

LogContext::
LogContext (int level, const char* file, const char* function, int line,
	    const char* tags) :
  _level(level), _file(file), _function(function), _line(line), 
  _tags(tags),
  _active(false)
{
  Synchronized sync(logger_mutex);
  log_points.push_back(this);
  LoggerPrivate::reconfig (--log_points.end());
}



LogContext::
~LogContext()
{
  Synchronized sync(logger_mutex);
  log_points_v::iterator it;
  it = find(log_points.begin(), log_points.end(), this);
  if (it != log_points.end())
    log_points.erase(it);
}

namespace {
    typedef map<string, int> level_strings_t;

    // class with constructor for initializing level_strings_t.
    struct level_strings_initializer
    {
        level_strings_initializer(): _map()
        {
            _map["emergency"] = LOGGER_EMERGENCY;
            _map["alert"] = LOGGER_ALERT;
            _map["critical"] = LOGGER_CRITICAL;
            _map["error"] = LOGGER_ERROR;
            _map["problem"] = LOGGER_PROBLEM;
            _map["warning"] = LOGGER_WARNING;
            _map["notice"] = LOGGER_NOTICE;
            _map["info"] = LOGGER_INFO;
            _map["debug"] = LOGGER_DEBUG;
        }
        level_strings_t _map;
    } _level_strings;

    level_strings_t& level_strings = _level_strings._map;
}

namespace nidas { namespace util {

int
stringToLogLevel(const std::string& slevel)
{
  string s = slevel; 
  for (unsigned int i = 0; i < s.length(); ++i)
    s[i] = tolower(s[i]);
  level_strings_t::iterator it = level_strings.find(s);
  if (it != level_strings.end())
    return it->second;
  return -1;
}

string
logLevelToString(int level)
{
  level_strings_t::iterator it;
  for (it = level_strings.begin(); it != level_strings.end(); ++it)
  {
    if (it->second == level)
      return it->first;
  }
  return "emergency";
}

}}
