// -*- mode: c++; c-basic-offset: 2; -*-
/*              Copyright 2004-2007 (C) by UCAR
 */

#ifndef NIDAS_UTIL_LOGGER_H_
#define NIDAS_UTIL_LOGGER_H_

#include <cstdarg>
#include <syslog.h>

#include <iostream>
#include <string>
#include <sstream>
#include <cstdio>
#include <vector>

#if !defined(SVR4) && ( defined(__GNUC__) && __GNUC__ < 2)
#include <varargs.h>
#endif

namespace nidas { namespace util {

/**
 * @defgroup Logging Logging
 *
 * Three main classes comprise the nidas::util logging facility:
 *
 *  - nidas::util::LogContext
 *  - nidas::util::Logger
 *  - nidas::util::LogConfig
 *
 * The intended use of this interface is through the @ref LoggerMacros.
 * These macros automatically instantiate the LogContext for the log point
 * and control the formatting and sending of the log message according to
 * the current logging configuration.  The advantage of the macros is that
 * they are less verbose, and there is no overhead formatting the message
 * parameters if the log message is not currently enabled.  However, the
 * original form of calling the Logger::log() method directly still works.
 * That form could be more natural in some cases.  It is equivalent to the
 * macros except the log message call cannot suppressed: the message itself
 * is suppressed within the log() method by checking the LogContext against
 * the current configuration.  The syslog macros like LOG_EMERG are
 * redefined to be a comma-separated list of the LogContext parameters, so
 * those parameters are passed into the log() call.  In other words,
 * consider original logging code that looks like this:
 *
 * @code
 * n_u::Logger::getInstance()->log(LOG_INFO,
 *    "~DSMEngine %s: %s",output->getName().c_str(),e.what());
 * @endcode
 *
 * LOG_INFO is expanded to this:
 *
 * @code
 * nidas::util::LOGGER_INFO, __FILE__, __PRETTY_FUNCTION__, __LINE__
 * @endcode
 *
 * So the log() call receives all of the context information it needs
 * to first of all compile a more complete log message, and second to
 * determine if the message has been enabled.
 *
 * The log() call above could be rewritten using a macro as follows:
 *
 * @code
 * ILOG(("~DSMEngine %s: %s",output->getName().c_str(),e.what()));
 * @endcode
 * 
 * LogConfig objects can be created to configure the set of log points
 * which will be active.  See @ref LoggerSchemes.
 *
 * NIDAS can configure the available logging schemes and the current scheme
 * through the XML config file.  Below is an excerpt from within the
 * <project> element.  The log scheme is set with the <logger> element.
 *
 * @todo A nice enhancement someday would be to set the log scheme name
 * from the dsm command-line.
 *
 * @code

  <logger scheme='everything' />

  <logscheme name='everything'>
    <showfields>all</showfields>
    <logconfig level='debug'/>
    <logconfig filematch='nidas'/>
    <logconfig/>
  </logscheme>

  <logscheme name='nothing'>
  </logscheme>

  <logscheme name='info'>
    <logconfig level='info'/>
  </logscheme>

  <logscheme name='iss-debug'>
    <showfields>level,time,function,message</showfields>
    <logconfig filematch='dynld/iss'/>
  </logscheme>

  <logscheme name='tilt-sensor-debug'>
    <logconfig filematch='dynld/iss'/>
    <logconfig filematch='dynld/psql'/>
    <logconfig filematch='nidas/core'/>
  </logscheme>

 * @endcode
 */
/**@{*/

/** @file Logger.h
 *
 * Header file for the nidas::util logging facility.
 **/

/**
 * @defgroup LogLevelSymbols Integer Logger Levels
 *
 * Define constants equal to the syslog levels, so we can redefine the
 * syslog symbols.
 **/
/**@{*/
const int LOGGER_EMERG = LOG_EMERG;
const int LOGGER_EMERGENCY = LOG_EMERG;
const int LOGGER_ALERT = LOG_ALERT;
const int LOGGER_CRIT = LOG_CRIT;
const int LOGGER_CRITICAL = LOG_CRIT;
const int LOGGER_ERROR = LOG_ERR;
const int LOGGER_ERR = LOG_ERR;
const int LOGGER_PROBLEM = LOG_ERR;
const int LOGGER_WARNING = LOG_WARNING;
const int LOGGER_NOTICE = LOG_NOTICE;
const int LOGGER_INFO = LOG_INFO;
const int LOGGER_DEBUG = LOG_DEBUG;
/**@}*/

#undef	LOG_EMERG
#undef	LOG_ALERT
#undef	LOG_CRIT
#undef	LOG_ERR
#undef	LOG_WARNING
#undef	LOG_NOTICE
#undef	LOG_INFO
#undef	LOG_DEBUG

#define LOG_CONTEXT(LEVEL) \
nidas::util::LEVEL, __FILE__, __PRETTY_FUNCTION__, __LINE__

// Redefine the log levels to be a full LogContext instance.
#define LOG_EMERG LOG_CONTEXT(LOGGER_EMERG)
#define	LOG_ALERT LOG_CONTEXT(LOGGER_ALERT)
#define	LOG_CRIT LOG_CONTEXT(LOGGER_CRIT)
#define	LOG_ERR LOG_CONTEXT(LOGGER_ERR)
#define	LOG_WARNING LOG_CONTEXT(LOGGER_WARNING)
#define	LOG_NOTICE LOG_CONTEXT(LOGGER_NOTICE)
#define	LOG_INFO LOG_CONTEXT(LOGGER_INFO)
#define	LOG_DEBUG LOG_CONTEXT(LOGGER_DEBUG)

#define LOGGER_LOGPOINT(LEVEL,MSG) \
do { static nidas::util::LogContext \
 nidas_util_log_context(nidas::util::LEVEL, __FILE__,__PRETTY_FUNCTION__,__LINE__); \
 if (nidas_util_log_context.active()) \
nidas::util::Logger::getInstance()->msg (nidas_util_log_context, \
nidas::util::LogMessage().format MSG); } \
while (0)

/**
 * @defgroup LoggerMacros Logging Macros
 *
 * These macros specify a log point in the code from which the given
 * message can be logged with the given log level.  The macro generates
 * code which first tests whether the log point is active before generating
 * the message, thus minimizing the time overhead for logging.  The @p MSG
 * argument must be itself in parentheses, so that it can be a variable
 * argument list.  The whole argument list is passed to
 * LogMessage::format() to generate the message string.
 *
 * @code
 * DLOG(("the current value of pi is %f", pi));
 * @endcode
 *
 * Since LogMessage::format() returns the LogMessage instance, further text
 * can be added to the message with the streaming operator<<, like so:
 *
 * @code
 * DLOG(("pi = ") << pi);
 * @endcode
 **/
/**@{*/
#define ELOG(MSG) LOGGER_LOGPOINT(LOGGER_EMERG,MSG)
#define ALOG(MSG) LOGGER_LOGPOINT(LOGGER_ALERT,MSG)
#define CLOG(MSG) LOGGER_LOGPOINT(LOGGER_CRITICAL,MSG)
#define PLOG(MSG) LOGGER_LOGPOINT(LOGGER_ERR,MSG) // For Problem, as in Error
#define WLOG(MSG) LOGGER_LOGPOINT(LOGGER_WARNING,MSG)
#define NLOG(MSG) LOGGER_LOGPOINT(LOGGER_NOTICE,MSG)
#define ILOG(MSG) LOGGER_LOGPOINT(LOGGER_INFO,MSG)
#define DLOG(MSG) LOGGER_LOGPOINT(LOGGER_DEBUG,MSG)
/**@}*/

class Logger;
class LoggerPrivate;

/**
 * Convert the name of a log level to its integer value.
 * @returns The log level value, or -1 if the name is not recognized.
 **/ 
int
stringToLogLevel(const std::string& slevel);

/**
 * Convert an integral log level to a string name.
 * @returns The name for the given log level, or else "emergency".
 **/ 
std::string
logLevelToString(int);

/**
 * The LogContext is created at a point in the application code and filled
 * in with details about that log point, such as the file and line number,
 * and the function name of the containing function, and the log level of
 * that log point, such as LOGGER_DEBUG or LOGGER_ERROR.  Except for the
 * log level, the rest of the LogContext can be filled in with CPP macros
 * and predefined compiler symbols like __PRETTY_FUNCTION__.  The
 * LogContext holds its own active state, so a simple, inline boolean test
 * determines whether a log point has been activated and thus needs to
 * generate and log its message.  Otherwise the log point can be skipped
 * with minimal time overhead.
 *
 * The LogContext effectively stashes context info for a log point so that
 * info can be used in the log message output, and so that info can be used
 * as criteria for selecting which messages are logged.  Thus a LogContext
 * registers itself with the Logger class, and the Logger class can enable
 * or disable a log point according to the current configuration scheme,
 * @ref LoggerSchemes.  The LogContext contains a flag which is false if
 * the log point is not active, meaning almost all overhead of generating a
 * log message can be avoided by testing the active() flag first.  The
 * typical usage of a LogContext will be as a static object, and the log
 * macros DLOG, ELOG, and so on test the active() flag before passing a
 * message.
 *
 * The ultimate goal is to make all log messages always available in the
 * runtime while incurring as little performance overhead as possible when
 * they are not need, and without requiring alternate compile
 * configurations.  However, if it should ever be necessary, then the log
 * macros can still be defined as completely empty.
 *
 * See LogContext::log() for using a LogContext instance to enclose more
 * complicated blocks of logging output.
 **/
class LogContext
{
public:

  /**
   * The full LogContext constructor.
   **/
  LogContext (int level, const char* file, const char* function, int line);

  ~LogContext();

  inline bool
  active() const
  {
    return _active;
  }

  inline const char*
  filename() const
  {
    return _file;
  }

  inline const char*
  function() const
  {
    return _function;
  }

  inline int
  line() const
  {
    return _line;
  }

  inline int
  level() const
  {
    return _level;
  }

  inline std::string
  levelName() const
  {
    return logLevelToString(_level);
  }

  /**
   * Convenience method which writes the given message to the current
   * Logger instance, passing this object as the LogContext instance.
   *
   * @code
   * static LogContext lp(LOG_INFO);
   * if (lp.active())
   * {
   *     LogMessage msg;
   *     msg << "complicated info output...";
   *     lp.log(msg);
   * }
   * @endcode
   **/
  void
  log(const std::string& msg) const;

private:
  int _level;
  const char* _file;
  const char* _function;
  int _line;

  bool _active;

  friend class nidas::util::Logger;
  friend class nidas::util::LoggerPrivate;
};
	

/**
 * A configuration to enable or disable a matching set of log points.  A
 * LogConfig specifies which log points to match and whether to activate or
 * disable them.  The set of log points are matched by filename, function
 * name, line number, and level.  For example, all the log messages in a
 * given file can be enabled with a LogConfig that sets @c filename_match
 * to the filename and @c level to @c LOGGER_DEBUG.  Methods within an
 * object all include the class name in the function signature, so those
 * can be enabled with a function_match set to the class name.  The default
 * LogConfig matches every log point.  File and function names are matched
 * as substrings, so a @c function_match set to "Logger::" will match all
 * methods of the Logger class.
 **/
class LogConfig
{
public:

  /**
   * Apply this config to log points in filenames which contain the 
   * given string.  An empty string matches everything.
   **/
  std::string filename_match;

  /**
   * Apply this config to log points in functions which contain the
   * given string.  An empty string matches everything.
   **/
  std::string function_match;

  /**
   * Apply this config to log points with the given line number.  Really
   * this only makes sense if filename_match is set also.  The default of
   * zero matches every line number.
   **/
  int line;

  /**
   * For all matching log points, enable the log point if the log level is
   * more severe (lower value) than this threshold level.  The default
   * threshold includes all log levels.
   **/
  int level;

  /**
   * If true, then all matching log points will be enabled.  If false, any
   * matching log points will be disabled.
   **/
  bool activate;

  /**
   * Return true if this config matches the given LogContext @p lc.
   **/
  inline bool
  matches(const LogContext& lc) const
  {
    return
      (filename_match.length() == 0 || 
	    strstr(lc.filename(), filename_match.c_str())) &&
      (function_match.length() == 0 || 
	    strstr(lc.function(), function_match.c_str())) &&
      (line == 0 || line == lc.line()) &&
      (lc.level() <= level);
  }

  /**
   * Construct a default LogConfig, which matches every log point.
   **/
  LogConfig() :
    line(0),
    level(LOGGER_DEBUG),
    activate(true)
  {}

};
  

/**
 * A LogScheme is a vector of LogConfig's and the vector of fields to show
 * in log messages.
 **/
class LogScheme
{
public:

  enum LogField 
  {
    NoneField = 0,
    ThreadField = 1, 
    FunctionField = 2, 
    FileField = 4, 
    LevelField = 8,
    MessageField = 16,
    TimeField = 32,
    AllFields = 63
  };

  /**
   * Convert the name of a log information field to its enum, such as
   * Logger::ThreadField.  Returns NoneField if the name is not recognized.
   **/
  static LogField
  stringToField(const std::string& s);

  /**
   * Convert a LogField to its lower-case string name.  Returns "none"
   * if the LogField is NoneField or invalid.
   **/
  static std::string
  fieldToString(LogScheme::LogField lf);

  /**
   * The default LogScheme has show fields set to "time,level,message" and
   * a single LogConfig to enable all messages with level LOGGER_WARNING
   * and above.  The default name is "default".  Explicitly passing an
   * empty name will also force a name of "default", since a LogScheme is
   * prohibited from having an empty name.
   **/
  explicit
  LogScheme(const std::string& name = "default");

  /**
   * LogScheme names must not be empty, so this method has no effect
   * unless name.length() > 0.
   **/
  LogScheme&
  setName(const std::string& name)
  {
    if (name.length() > 0)
      _name = name;
    return *this;
  }

  const std::string&
  getName() const
  {
    return _name;
  }

  /**
   * Clear the set of LogConfig's in this scheme.  An empty set of
   * LogConfig's disables all logging for this scheme.
   **/
  LogScheme&
  clearConfigs();

  /**
   * Push a new LogConfig onto the list of existing configs.  This config
   * will be applied to any log points which it matches.  For example, if
   * the @c activate flag of this config is false, then any matching log
   * points will be disabled.  The configurations are cumulative, so the
   * latest config on the list will take precedence.  For example, if an
   * earlier config enables all points, then further configs can disable a
   * subset of log points.
   *
   * @param lc The LogConfig to be added to this scheme.
   * @returns This scheme.
   **/
  LogScheme&
  addConfig(const LogConfig& lc);

  /**
   * Set the fields to show in log messages and their order.  The fields
   * in @p fields will be shown in the same order as in the vector.  Any
   * fields not included will be omitted.
   *
   * @param fields A vector<LogField>.
   **/
  LogScheme&
  setShowFields(const std::vector<LogField>& fields);

  /**
   * Parse a comma-separated string of field names, with no spaces,
   * into a vector of LogField values, and pass that to setShowFields().
   **/
  LogScheme&
  setShowFields(const std::string& fields);

  /**
   * Return a comma-separated string describing the list of fields
   * to be shown by this scheme.
   **/
  std::string
  getShowFieldsString () const;

private:

  std::string _name;
  typedef std::vector<LogConfig> log_configs_v;
  log_configs_v log_configs;
  std::vector<LogField> log_fields;

  friend class nidas::util::LoggerPrivate;
  friend class nidas::util::Logger;
};


/**
 * A class for formatting and streaming a log message.  Text can be
 * appended to the message with a printf() format, or streamed with the
 * stream output operator<<.
 **/
class LogMessage
{
public:
  LogMessage(const std::string& s = "") : msg(s)
  {
  }

  LogMessage&
  format(const char *fmt, ...);

  const std::string&
  getMessage() const
  {
    return msg;
  }

  operator const std::string& () const
  {
    return msg;
  }

  std::string msg;
};


template <typename T>
LogMessage&
operator<< (LogMessage& lf, const T& t)
{
  std::ostringstream oss;
  oss << t;
  lf.msg += oss.str();
  return lf;
}

template <typename T>
LogMessage&
operator<< (LogMessage& lf, const std::string& t)
{
  lf.msg += t;
  return lf;
}


/**
 * Simple logging class, based on UNIX syslog interface.  The Logger is a
 * singleton through which all log messages are sent.  It determines
 * whether messages are sent to syslog or written to an output stream
 * provided by the application, such as std::cerr, or a file output stream,
 * or an ostringstream.
 */
class Logger {
protected:
  Logger(const char *ident, int logopt, int facility, const char *TZ);
  Logger(std::ostream* );
  Logger();

public:

  ~Logger();

  /** 
   * Create a syslog-type Logger. See syslog(2) man page.
   * @param ident: see syslog parameter
   * @param logopt: see syslog parameter
   * @param facility: see syslog parameter
   * @param TZ: string containing timezone for syslog time strings
   *        If NULL(0), use default timezone.
   */
  static Logger* createInstance(const char *ident, int logopt, int facility,
  	const char *TZ = 0);

  /**
   * Create a logger to the given output stream.  The output stream should
   * remain valid as long as this logger exists.  This Logger does not take
   * responsibility for closing or destroying the stream.
   *
   * @param out Pointer to the output stream.
   **/
  static Logger* createInstance(std::ostream* out);

  /**
   * Return a pointer to the currently active Logger singleton.
   **/
  static Logger* getInstance();

  /**
   * Build a message from a printf-format string and variable args, and log
   * the message.  The severity should be passed using one of the syslog
   * macros, such as @c LOG_DEBUG or @c LOG_EMERG.  Those macros are
   * redefined such that they fill in the first four parameters
   * automatically:
   *
   * @code
   * Logger::getInstance()->log (LOG_DEBUG, "pi=%f", 3.14159);
   * @endcode
   *
   * This method is mostly for backwards compatibility.  The newer log
   * method is to use the log context macros @c DLOG, @c ILOG, @c ELOG, and
   * so on, since those macros will not waste time generating the message
   * if the message will not be logged.
   **/
#if defined(SVR4) || ( defined(__GNUC__) && __GNUC__ > 1 )
  void
  log(int severity, const char* file, const char* fn,
      int line, const char *fmt, ...);
#elif defined(__GNUC__)
  void
  log(...);
#else
  void
  log(va_alist);
#endif

  /**
   * Send a log message for the given LogContext @p lc.  No check is done
   * for whether the context is active or not, the message is just
   * immediately sent to the current log output, formatted according to the
   * context.  For syslog output, the message and severity level are
   * passed.  For all other output, the message includes the current time
   * and the log context info, such as filename, line number, function
   * name, and thread name.
   **/
  void
  msg (const LogContext& lc, const std::string& msg);

  void
  msg (const LogContext& lc, const LogMessage& m)
  {
    msg (lc, m.getMessage());
  }

  /**
   * @defgroup LoggerSchemes Logging Configuration Schemes
   * 
   * The @ref Logging facility can store multiple logging configuration
   * schemes, each with a particular name.  Each scheme is a list of
   * LogConfig objects.  These methods are used to manipulate the set of
   * schemes known by the Logger.  At any time the logging configuration
   * can switch schemes by calling setScheme().  The first time a new
   * scheme name is referenced, a new scheme is created with an empty list
   * of configs, which disables all logging in that scheme.
   **/
  /**@{*/


  /**
   * Set the current scheme.  If a scheme named @p name does not exist and
   * @p name is non-empty, a default scheme is created with this name.
   * Trying to set the current scheme to an empty name has no effect.
   **/
  void
  setScheme(const std::string& name);

  /**
   * Set the current scheme to the given @p scheme.  The scheme is first
   * added with updateScheme(), then it becomes the current scheme.
   **/
  void
  setScheme(const LogScheme& scheme);

  /**
   * Update or insert this scheme in the collection of log configuration
   * schemes.  If a scheme already exists with the same name, this scheme
   * will replace it.  If the given scheme has the same name as the
   * Logger's current scheme, then the Logger's scheme will be updated
   * also.
   **/
  void
  updateScheme(const LogScheme& scheme);

  /**
   * Get a copy of the scheme with the given @p name.  If the name does not
   * exist, a default scheme is returned.  If the name is empty, then
   * the current scheme is returned.
   **/
  LogScheme
  getScheme(const std::string& name = "") const;

  /**@}*/


protected:
  /**
   * Set the timezone to be used in the log messages, whether passed to
   * syslog or formatted to include the current time.
   *
   * To be careful, don't make this public. The user should set the
   * TZ once in the constructor. Otherwise, in a multithreaded app
   * the log() method could have problems, since this is a singleton
   * shared by multiple threads, and we don't provide a locking
   * mechanism for loggerTZ.
   */
  void setTZ(const char* val);

  static Logger* _instance;

  std::ostream* output;
  bool syslogit;

  char* loggerTZ;
  char* saveTZ;
};

inline void
LogContext::
log(const std::string& msg) const
{
  Logger::getInstance()->msg (*this, msg);
}

/**@}*/

}}	// namespace nidas namespace util

#endif
