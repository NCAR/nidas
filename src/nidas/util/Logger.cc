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
#include <cstring>
#include <cstdarg>
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


// Keep the Mutex in the local anonymous namespace where it can be seen and
// accessed only by the implmentations in this module.
namespace {
  Mutex& get_logger_mutex()
  {
    static nidas::util::Mutex logger_mutex;
    return logger_mutex;
  }
};


/// Vector of pointers to all the LogContext instances initialized so far.
static log_points_v log_points;
/// Map of all the known LogScheme instances.
static log_schemes_t log_schemes;

static LogScheme get_scheme(const std::string& name);

// Keep a copy of the current scheme separate from the scheme map, so it
// does not need to be looked up every time a message needs to be logged.
static LogScheme current_scheme = get_scheme("");


/**
 * This must be called while the mutex is locked, since it accesses the
 * shared LogScheme map, and it returns a pointer which might change once
 * the lock is released.
 */
static LogScheme*
lookup_scheme(const std::string& name)
{
  log_schemes_t::iterator it = log_schemes.find(name);
  if (it == log_schemes.end())
    return 0;
  return &(it->second);
}

/**
 * Get a scheme with the name, or return a default.  Must be called with
 * the mutex locked.
 */
static LogScheme
get_scheme(const std::string& name)
{
  LogScheme* lsp = lookup_scheme(name);
  if (!lsp)
  {
    LogScheme df(name);
    // May as well make it a fallback config, in case an app ever wants to
    // be able to override the default config.  Just in case level_strings
    // has not been initialized yet, do not try to parse level as a string,
    // just set the int level directly.
    LogConfig lc;
    lc.level = LOGGER_WARNING;
    df.addFallback(lc);
    log_schemes[name] = df;
  }
  return log_schemes[name];
}


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

class LoggerPrivate
{
public:
  static const int DEFAULT_LOGLEVEL = LOGGER_NOTICE;

  // Return the appropriate setting for the given context's active flag
  // according to all the log configs in the given scheme.  Note that this does
  // not use any objects other than the LogScheme and LogContext passed into it,
  // so if a lock is needed to guard those objects, it must be locked before
  // calling this function.
  static bool
  get_active_flag(const LogScheme& scheme, LogContext* lc)
  {
    bool active = false;
    const LogScheme::log_configs_v& log_configs = scheme.log_configs;
    auto firstconfig = log_configs.begin();
    auto ic = firstconfig;

    // If there are no configs, then check it against the default level.
    if (firstconfig == log_configs.end())
    {
      active = (lc->_level <= DEFAULT_LOGLEVEL);
    }
    for ( ; ic != log_configs.end(); ++ic)
    {
      if (ic->matches(*lc))
      {
        if (DEBUG_LOGGER)
        {
          cerr << "reconfig: =="
               << (ic->activate ? " on" : "off") << "==> "
               << lc->_file << ":" << lc->_line
               << ":" << lc->_function << ":" << lc->levelName() << endl;
        }
        active = ic->activate;
      }
    }
    return active;
  }

  static void
  reconfig(const LogScheme& scheme, const log_points_v& points)
  {
    // This does not lock the logger_mutex, because it does not use shared
    // objects unless they are passed in, in which case locking is up to the
    // caller.
    //
    if (DEBUG_LOGGER)
    {
      stream_backtrace (cerr);
      cerr << "current scheme: " << scheme.getName() << "\n";
      cerr << " - " << scheme.log_configs.size() << " configs\n"
           << " - showfields: " 
           << scheme.getShowFieldsString() << "\n";
      cerr << "currently " << points.size() << " log points.\n";
    }
    for (auto& lcp: points)
    {
      lcp->setActive(get_active_flag(scheme, lcp));
    }
  }
};

}} // nidas::util


//================================================================
// Logger
//================================================================

Logger::Logger(const std::string& ident, int logopt, int facility,
               const char *TZ):
	_output(0), _syslogit(true), _loggerTZ(0), _saveTZ(0), _ident(0)
{
  // Stash the identity in a char array tied to the lifetime of the Logger.
  _ident = new char[ident.size() + 1];
  strcpy(_ident, ident.c_str());

  // To test the test that tests the memory tests:
  // _ident = const_cast<char*>(ident.c_str());

  // open syslog connection
  ::openlog(_ident, logopt, facility);
  setTZ(TZ);
}

Logger::Logger(std::ostream* out) : 
  _output(out), _syslogit(false), _loggerTZ(0), _saveTZ(0), _ident(0)
{
}

Logger::Logger():
    _output(&cerr),
    _syslogit(false),
    _loggerTZ(0),
    _saveTZ(0),
    _ident(0)
{
}

Logger::~Logger() {
  if (_syslogit) ::closelog();
  if (_output) _output->flush();
  delete [] _loggerTZ;
  delete [] _saveTZ;
  delete [] _ident;
}


/* static */
void
Logger::
destroyInstance()
{
  Synchronized sync(get_logger_mutex());
  delete _instance;
  _instance = 0;
}

/* static */
void
Logger::
init()
{
    // just access the mutex so the static allocation goes onto the
    // finalization stack before any LogContext instances are created. this
    // may help assure that no LogContext destructor accesses the mutex after
    // it has been destroyed.
    get_logger_mutex();
}

/* static */
Logger* Logger::_instance = 0;

/* static */
Logger* 
Logger::
createInstance(const std::string& ident, int logopt, int facility,
               const char *TZ)
{
  Synchronized sync(get_logger_mutex());
  delete _instance;
  _instance = new Logger(ident, logopt, facility, TZ);
  return _instance;
}

/* static */
Logger*
Logger::
get_instance_locked(std::ostream* out)
{
  if (!_instance)
  {
    if (!out)
      out = &cerr;
    _instance = new Logger(out);
  }
  return _instance;
}

/* static */
Logger* Logger::createInstance(std::ostream* out) 
{
  Synchronized sync(get_logger_mutex());
  delete _instance;
  _instance = 0;
  return get_instance_locked(out);
}

/* static */
Logger* Logger::getInstance()
{
  if (!_instance)
    createInstance();
  return _instance;
}

void Logger::setTZ(const char* val) {

  delete [] _loggerTZ;
  _loggerTZ = 0;

  delete [] _saveTZ;
  _saveTZ = 0;

  if (!val) return;	// user wants default TZ

  const char *tz;
  if (!(tz = getenv("TZ"))) tz = "GMT";

  // it is risky to use string.c_str() for these TZ
  // environment varibles. The pointer must always be valid
  // so we use simple char*.
  if (strcmp(val,tz)) {
    _loggerTZ = new char[strlen(val) + 4];
    strcpy(_loggerTZ,"TZ=");
    strcat(_loggerTZ,val);

    _saveTZ = new char[strlen(tz) + 4];
    strcpy(_saveTZ,"TZ=");
    strcat(_saveTZ,tz);
  }
}


void
Logger::
msg(const nidas::util::LogContextState& lc, const std::string& msg)
{
  static const char* fixedsep = "|";
  Synchronized sync(get_logger_mutex());

  // Double-check that the context is enabled.  It's a simple check, and it
  // guards against code accidentally logging a message to a context
  // without using a macro that checks automatically.
  //
  // Probably it is a bad idea to send VERBOSE messages to syslog(), so
  // trap that here too.
  if (!lc.active() || (_syslogit && lc.level() == LOGGER_VERBOSE))
  {
    return;
  }

  if (_loggerTZ) {
    putenv(_loggerTZ);
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
    if (_syslogit)
    {
      syslog(lc.level(), "%s", oss.str().c_str());
    }
    else
    {
      // We want to flush the stream as well write a newline, in the hopes
      // of keeping log messages intact.
      *_output << oss.str() << std::endl;
    }
  }
  if (_loggerTZ) {
    putenv(_saveTZ);
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


void Logger::log(int severity, const char* file, const char* fn,
		 int line, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

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
  Synchronized sync(get_logger_mutex());
  current_scheme = get_scheme(name);
  LoggerPrivate::reconfig(current_scheme, log_points);
}


void
Logger::
setScheme(const LogScheme& scheme)
{
  Synchronized sync(get_logger_mutex());
  log_schemes[scheme.getName()] = scheme;
  current_scheme = scheme;
  LoggerPrivate::reconfig(current_scheme, log_points);
}


LogScheme
Logger::
getScheme(const std::string& name)
{
  Synchronized sync(get_logger_mutex());
  return get_scheme(name);
}


LogScheme
Logger::
getScheme()
{
  Synchronized sync(get_logger_mutex());
  return current_scheme;
}


bool
Logger::
knownScheme(const std::string& name)
{
  Synchronized sync(get_logger_mutex());
  return bool(lookup_scheme(name));
}


void
Logger::
updateScheme(const LogScheme& scheme)
{
  Synchronized sync(get_logger_mutex());
  log_schemes[scheme.getName()] = scheme;
  if (current_scheme.getName() == scheme.getName())
  {
    current_scheme = scheme;
    LoggerPrivate::reconfig(current_scheme, log_points);
  }
}


void
Logger::
clearSchemes()
{
  Synchronized sync(get_logger_mutex());
  log_schemes.clear();
  current_scheme = get_scheme("");
  // resetting the current scheme also resets log points.
  LoggerPrivate::reconfig(current_scheme, log_points);
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
  parse(text);
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


LogConfig&
LogConfig::
parse(const std::string& fields)
{
  // Break the string at commas, then parse the fields as <field>=<value>.
  string::size_type at = 0;
  do {
    string::size_type comma = fields.find(',', at);
    if (comma == string::npos) 
      comma = fields.length();

    std::string field = fields.substr(at, comma-at);
    at = comma+1;

    string::size_type equal = field.find('=');
    string value = field;
    if (equal != string::npos)
    {
      ++equal;
      value = field.substr(equal, field.length()-equal);
      field = field.substr(0, equal-1);
    }
    if (field == "")
    {
      // Allow an empty string or field, but nothing changes.
    }
    else if (field == "enable")
    {
      this->activate = true;
    }
    else if (field == "disable")
    {
      this->activate = false;
    }
    else if (equal == string::npos || field == "level")
    {
      // No equal sign and not enable or disable, so this must be a log
      // level.
      if (!parse_log_level(this->level, value))
      {
        throw std::runtime_error("unknown log level: " + value);
      }
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
      {
        throw std::runtime_error("log config line number must be "
                                 "positive: " + value);
      }
      this->line = line;
    }
    else
    {
      throw std::runtime_error("unknown log config field: " + field);
    }
  }
  while (at < fields.length());
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


static LogScheme::LogField
default_fields[] = { 
  LogScheme::TimeField,
  LogScheme::LevelField,
  LogScheme::MessageField
};


LogScheme::
LogScheme(const std::string& name) :
  _name(name),
  log_configs(),
  log_fields(default_fields, default_fields+3),
  _parameters(),
  _showlogpoints(false),
  _fallbacks(false)
{
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
  // Parse the string for field names and construct a vector from them. If the
  // string is empty, no fields are shown.
  std::vector<LogField> fv;
  string::size_type at = 0;
  while (at < fields.length())
  {
    string::size_type comma = fields.find(',', at);
    if (comma == string::npos) 
      comma = fields.length();
    string field = fields.substr(at, comma-at);
    LogField f = stringToField(field);
    if (f == NoneField)
    {
      throw std::runtime_error("unknown log message field: " + field);
    }
    fv.push_back(f);
    at = comma+1;
  }
  return setShowFields(fv);
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
  if (_fallbacks)
  {
    log_configs.clear();
    _fallbacks = false;
  }
  log_configs.push_back(lc);
  return *this;
}


LogScheme&
LogScheme::
addConfig(const std::string& config)
{
  return addConfig(LogConfig(config));
}


LogScheme&
LogScheme::
addFallback(const LogConfig& lc)
{
  if (_fallbacks || log_configs.empty())
  {
    log_configs.push_back(lc);
    _fallbacks = true;
  }
  return *this;
}


LogScheme&
LogScheme::
addFallback(const std::string& config)
{
  return addFallback(LogConfig(config));
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


static LogContext
show_point(LOG_STATIC_CONTEXT(LOGGER_INFO), "show_log_points");


/**
 * Log a message about the existence and state of the given LogContext @lp. Like
 * other log messages, this one happens without a lock of the logger_mutex.
 */
void
LogScheme::
show_log_point(const LogContextState& lp) const
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
  show_point.log(buf.str());
}


void
LogScheme::
showLogPoints(bool show)
{
  // Set the flag to show log points, and in case some log points were already
  // created, show those now.  Rather than hold the lock to log them all, send
  // copies of the log points.
  _showlogpoints = show;
  if (_showlogpoints)
  {
    // Copy the LogContext objects while locked, then iterate through them
    // calling show_log_point without the lock.  show_log_point() only needs the
    // copy to log information about the log point, and it's safe for a
    // LogContext to go away in another thread even while the copy is being
    // logged.
    std::vector<LogContextState> points;
    get_logger_mutex().lock();
    for (auto& lcp: log_points)
    {
      points.emplace_back(*lcp);
    }
    get_logger_mutex().unlock();
    for (auto& lc: points)
    {
      show_log_point(lc);
    }
  }
}


//================================================================
// LogContext
//================================================================

LogContextState::
LogContextState(int level, const char* file, const char* function,
                int line, const char* tags) :
  _level(level), _file(file), _function(function), _line(line), 
  _tags(tags),
  _active(false),
  _threadId(Thread::currentThreadId())
{}

LogContext::
LogContext(int level, const char* file, const char* function, int line,
           const char* tags) :
  LogContextState(level, file, function, line, tags)
{
  LogScheme scheme;
  {
    Synchronized sync(get_logger_mutex());
    log_points.push_back(this);
    scheme = current_scheme;
    LoggerPrivate::reconfig(scheme, { this });
  }
  // The lock is not needed to call show_log_point().
  if (scheme.getShowLogPoints())
  {
    scheme.show_log_point(*this);
  }
}


#if NIDAS_LOGGER_GUARD_ACTIVE
bool
LogContext::active() const
{
  Synchronized sync(get_logger_mutex());
  return _active;
}

void
LogContext::setActive(bool active)
{
  // this is only called while the lock is already held, since otherwise this
  // might write to memory for a LogContext which no longer exists.
  _active = active;
}
#endif


std::string
LogContextState::
threadName() const
{
    Thread* thread = Thread::lookupThread(_threadId);
    return !thread ? "unknown" : thread->getName();
}


LogContext::
~LogContext()
{
  Synchronized sync(get_logger_mutex());
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
  // Accept shorter form for warning.
  if (s == "warn")
    return LOGGER_WARNING;
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
