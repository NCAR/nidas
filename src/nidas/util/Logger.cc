// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2; -*-
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
#include <stdexcept>

#include <cctype>
#include <cstdlib>

using namespace nidas::util;
using namespace std;

//================================================================
// LoggerPrivate
//================================================================

typedef vector<LogContext*> log_points_v;
typedef map<string,LogScheme> log_schemes_t;

Mutex Logger::mutex;

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
  static const int DEFAULT_LOGLEVEL = LOGGER_NOTICE;

  // Return the appropriate setting for the given context's active flag
  // according to all the current log configs.
  static bool
  get_active_flag(LogContext* lc, bool& matched)
  {
    bool active = false;
    matched = false;

    LogScheme::log_configs_v& log_configs = current_scheme.log_configs;
    LogScheme::log_configs_v::iterator firstconfig = log_configs.begin();
    LogScheme::log_configs_v::iterator ic;

    // If there are no configs, then check it against the default level.
    if (firstconfig == log_configs.end())
    {
      active = (lc->_level <= DEFAULT_LOGLEVEL);
    }
    for (ic = firstconfig ; ic != log_configs.end(); ++ic)
    {
      if (ic->matches(*lc))
      {
        if (DEBUG_LOGGER)
        {
          cerr << "reconfig: =="
               << (ic->activate ? " on" : "off") << "==> "
               << lc->_file << ":" << lc->_line
               << ":" << lc->_function << endl;
        }
        matched = true;
        active = ic->activate;
      }
    }
    return active;
  }

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
    log_points_v::iterator it;
    for (it = firstpoint ; it != log_points.end(); ++it)
    {
      bool matched;
      bool active = get_active_flag((*it), matched);
      (*it)->_active = active;
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

Logger::Logger(std::ostream* out) : 
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
  Synchronized sync(Logger::mutex);
  if (_instance) {
      delete _instance;
  }
  _instance = new Logger(ident,logopt,facility,TZ);
  return _instance;
}

/* static */
Logger* Logger::createInstance(std::ostream* out) 
{
  Synchronized sync(Logger::mutex);
  if (_instance) {
      delete _instance;
  }
  _instance = new Logger(out);
  return _instance;
}

/* static */
Logger* Logger::getInstance() {
  if (!_instance)
      return createInstance(&cerr);
  else
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
msg(const nidas::util::LogContext& lc, const std::string& msg)
{
  Synchronized sync(Logger::mutex);
  msg_locked(lc, msg);
}


void
Logger::
msg_locked(const nidas::util::LogContext& lc, const std::string& msg)
{
  static const char* fixedsep = "|";

  // Double-check that the context is enabled.  It's a simple check, and it
  // guards against code accidentally logging a message to a context
  // without using a macro that checks automatically.
  //
  // Probably it is a bad idea to send VERBOSE messages to syslog(), so
  // trap that here too.
  if (!lc.active() || (syslogit && lc.level() == LOGGER_VERBOSE))
  {
    return;
  }

  if (loggerTZ) {
    putenv(loggerTZ);
    tzset();
  }
  ostringstream oss;

  const char* sep = "";
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
      now.setUTC(false);
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
      // We want to flush the stream as well write a newline, in the hopes
      // of keeping log messages intact.
      *output << oss.str() << std::endl;
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
setScheme(const std::string& name)
{
  if (name.length() > 0)
  {
    Synchronized sync(Logger::mutex);
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
  Synchronized sync(Logger::mutex);
  log_schemes[scheme.getName()] = scheme;
  current_scheme = scheme;
  LoggerPrivate::reconfig(log_points.begin());
}


LogScheme
Logger::
getScheme(const std::string& name) const
{
  Synchronized sync(Logger::mutex);
  if (name.length() == 0)
    return current_scheme;
  log_schemes[name].setName(name);
  return log_schemes[name];
}


void
Logger::
updateScheme(const LogScheme& scheme)
{
  Synchronized sync(Logger::mutex);
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
  this->msg << buffer;
  return *this;
}


//================================================================
// LogConfig
//================================================================


LogConfig::
LogConfig(const std::string& text) :
  filename_match(),
  function_match(),
  tag_match(),
  line(0),
  level(LOGGER_DEBUG),
  activate(true)
{
  if (!parse(text))
  {
    throw std::runtime_error("LogConfig error parsing: " + text);
  }
}


bool
LogConfig::
matches(const LogContext& lc) const
{
  return
    (filename_match.length() == 0 || lc.filename() == 0 ||
     std::strstr(lc.filename(), filename_match.c_str())) &&
    (function_match.length() == 0 || lc.function() == 0 ||
     std::strstr(lc.function(), function_match.c_str())) &&
    (tag_match.length() == 0 || lc.tags() == 0 ||
     std::strstr(lc.tags(), tag_match.c_str())) &&
    (line == 0 || line == lc.line()) &&
    (lc.level() <= level);
}


bool
parse_log_level(int& level_out, const std::string& text)
{
  if (text.empty())
    return false;
  std::string arg(text);
  const char* start = arg.c_str();
  char* end;
  int level = ::strtol(start, &end, 0);
  if (*end == '\0')
  {
    // Integer parse succeeded, check for valid range.
    if (level <= LOGGER_NONE || level > LOGGER_VERBOSE)
      return false;
  }
  else
  {
    level = nidas::util::stringToLogLevel(arg);
    if (level == LOGGER_NONE)
      return false;
  }
  level_out = level;
  return true;
}

bool
LogConfig::
parse(const std::string& fields)
{
  // An empty string is specifically allowed, but it changes nothing.
  if (fields.empty())
    return true;

  // Break the string at commas, then parse the fields as <field>=<value>.
  string::size_type at = 0;
  do {
    string::size_type comma = fields.find(',', at);
    if (comma == string::npos) 
      comma = fields.length();

    std::string field = fields.substr(at, comma-at);
    at = comma+1;

    string::size_type equal = field.find('=');
    string value;
    if (equal != string::npos)
    {
      ++equal;
      value = field.substr(equal, field.length()-equal);
      field = field.substr(0, equal-1);
    }
    if (field == "enable")
    {
      this->activate = true;
    }
    else if (field == "disable")
    {
      this->activate = false;
    }
    else if (equal == string::npos)
    {
      // No equal sign and not enable or disable, so this must be a log
      // level.
      if (!parse_log_level(this->level, field))
        return false;
    }
    else if (field == "level")
    {
      if (!parse_log_level(this->level, value))
        return false;
    }
    else if (field == "file")
    {
      this->filename_match = value;
    }
    else if (field == "function")
    {
      this->function_match = value;
    }
    else if (field == "tag")
    {
      this->tag_match = value;
    }
    else if (field == "line")
    {
      int line = atoi(value.c_str());
      if (line < 1)
        return false;
      this->line = line;
    }
    else
    {
      return false;
    }
  }
  while (at < fields.length());
  return true;
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


LogScheme
LogScheme::
current()
{
  return nidas::util::Logger::getInstance()->getScheme();
}


LogScheme::
LogScheme(const std::string& name) :
  _name(name), log_configs(), log_fields(), _parameters(),
  _showlogpoints(false)
{
  // Force an empty name to the default, non-empty name.
  if (name.length() == 0)
  {
    _name = LogScheme().getName();
  }
  setShowFields("time,level,message");
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


LogScheme::log_configs_v
LogScheme::
getConfigs()
{
  return log_configs;
}


int
LogScheme::
logLevel()
{
  LogScheme::log_configs_v::iterator ic;
  if (log_configs.empty())
  {
    return LoggerPrivate::DEFAULT_LOGLEVEL;
  }
  int level = LOGGER_NONE;
  for (ic = log_configs.begin(); ic != log_configs.end(); ++ic)
  {
    level = std::max(level, ic->level);
  }
  return level;
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


bool
LogScheme::
parseParameter(const std::string& text)
{
  string::size_type equal = text.find('=');
  if (equal == string::npos)
  {
    return false;
  }
  ++equal;
  string name = text.substr(0, equal-1);
  string value = text.substr(equal, text.length()-equal);
  if (name.empty())
  {
    return false;
  }
  setParameter(name, value);
  return true;
}


void
LogScheme::
setParameter(const std::string& name, const std::string& value)
{
  _parameters[name] = value;
}


std::string
LogScheme::
getParameter(const std::string& name, const std::string& dvalue)
{
  std::map<std::string, std::string>::iterator it;
  it = _parameters.find(name);
  if (it != _parameters.end())
  {
    return it->second;
  }
  return dvalue;
}


std::string
LogScheme::
getEnvParameter(const std::string& name, const std::string& dvalue)
{
  std::map<std::string, std::string>::iterator it;
  it = _parameters.find(name);
  if (it != _parameters.end())
  {
    return it->second;
  }
  std::string value = dvalue;
  const char* ev = std::getenv(name.c_str());
  if (ev)
    value = ev;
  return value;
}


static LogContext show_point(LOG_INFO, "show_log_points");

void
LogScheme::
show_log_point(LogContext& lp)
{
  std::ostringstream buf;
  const char* tags = "";
  if (lp.tags())
  {
    tags = lp.tags();
  }
  buf 
    << "Show log point: "
    << lp.levelName() << "[" << tags << "]"
    << " in " << lp.function() << "@"
    << lp.filename() << ":" << lp.line()
    << " is" << (lp.active() ? "" : " not") << " active";
  Logger::getInstance()->msg_locked(show_point, buf.str());
}


void
LogScheme::
showLogPoints(bool show)
{
  _showlogpoints = show;
  if (_showlogpoints)
  {
    // Do not to create any LogContext in this block or otherwise modify
    // the log points vector while it's locked.
    Synchronized sync(Logger::mutex);
    log_points_v::iterator it;
    for (it = log_points.begin(); it != log_points.end(); ++it)
    {
      show_log_point(*(*it));
    }
  }
}


//================================================================
// LogContext
//================================================================

LogContext::
LogContext (int level, const char* file, const char* function, int line,
	    const char* tags) :
  _level(level), _file(file), _function(function), _line(line), 
  _tags(tags),
  _active(false),
  _threadId(Thread::currentThreadId())
{
  bool matched;
  {
    Synchronized sync(Logger::mutex);
    log_points.push_back(this);
    // At one point the active flag was set outside the critical section,
    // so concurrency checkers would not complain when later on the same
    // flag was tested by other threads without a lock.  However, in case
    // the log point will be shown by the current log scheme, the flag
    // should be set before showing it.  And it seemed excessive to release
    // the lock just to set the flag and then lock again to show the log
    // point.
    _active = LoggerPrivate::get_active_flag(this, matched);
    if (current_scheme.getShowLogPoints())
    {
      current_scheme.show_log_point(*this);
    }
  }
}


std::string
LogContext::
threadName() const
{
    Thread* thread = Thread::lookupThread(_threadId);
    if (!thread)
    {
        return "unknown";
    }
    return thread->getName();
}


LogContext::
~LogContext()
{
  Synchronized sync(Logger::mutex);
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
            _map["verbose"] = LOGGER_VERBOSE;
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
  return LOGGER_NONE;
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
  return "none";
}

}}
