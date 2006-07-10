/*              Copyright 2004 (C) by UCAR
 *
 * File       : $RCSfile: Logger.h,v $
 * Revision   : $Revision: 1.4 $
 * Directory  : $Source: /code/cvs/isa/src/lib/atdUtil/Logger.h,v $
 * System     : PAM
 * Date       : $Date: 2005/01/18 00:13:52 $
 *
 * Description:
 *
 */

#ifndef NIDAS_UTIL_LOGGER_H_
#define NIDAS_UTIL_LOGGER_H_

/*
#include <cerrno>
#include <cstring>
*/

#include <cstdarg>
#include <syslog.h>

#include <iostream>
#include <string>
#include <cstdio>

#if !defined(SVR4) && ( defined(__GNUC__) && __GNUC__ < 2)
#include <varargs.h>
#endif

namespace nidas { namespace util {

/**
 * Simple logging class, based on UNIX syslog interface.
 * Messages are either sent to the syslog daemon or to a
 * C FILE* pointer (e.g. stderr).
 */
class Logger {
protected:
  Logger(const char *ident, int logopt, int facility, const char *TZ);
  Logger(FILE* );
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

  static Logger* createInstance(FILE*);

  static Logger* getInstance();

#if defined(SVR4) || ( defined(__GNUC__) && __GNUC__ > 1 )
  void log(int severity, const char *fmt, ...);
#elif defined(__GNUC__)
  void log(...);
#else
  void log(va_alist);
#endif

protected:
  /**
   * Set the timezone to be used in the syslog messages, which
   * contain the current time.
   * To be careful, don't make this public. The user should set the
   * TZ once in the constructor. Otherwise, in a multithreaded app
   * the log() method could have problems, since this is a singleton
   * shared by multiple threads, and we don't provide a locking
   * mechanism for loggerTZ.
   */
  void setTZ(const char* val);

  static Logger* _instance;

  FILE* output;
  bool syslogit;

  char* loggerTZ;
  char* saveTZ;

};

}}	// namespace nidas namespace util

#endif
