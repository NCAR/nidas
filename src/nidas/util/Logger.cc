/*              Copyright 2004 (C) by UCAR
 *
 * File       : $RCSfile: Logger.cpp,v $
 * Revision   : $Revision: 1.5 $
 * Directory  : $Source: /code/cvs/isa/src/lib/atdUtil/Logger.cpp,v $
 * System     : PAM
 * Date       : $Date: 2005/01/18 00:13:52 $
 *
 * Description:
 *
 */

#include <nidas/util/Logger.h>

#include <string>
#include <cerrno>

using namespace nidas::util;
using namespace std;

Logger::Logger(const char *ident, int logopt, int facility, const char *TZ):
	output(0),syslogit(true),loggerTZ(0),saveTZ(0) {
  // open syslog connection
  ::openlog(ident,logopt,facility);
  setTZ(TZ);
}
Logger::Logger(FILE* out) : output(out),syslogit(false),loggerTZ(0),saveTZ(0) {
}

Logger::Logger() : output(stderr),syslogit(false),loggerTZ(0),saveTZ(0) {
}

Logger::~Logger() {
  if (syslogit) ::closelog();
  if (output) ::fflush(output);
  delete [] loggerTZ;
  delete [] saveTZ;
  _instance = 0;
}

/* static */
Logger* Logger::_instance = 0;

/* static */
Logger* Logger::createInstance(const char *ident, int logopt, int facility, const char *TZ) {
  if (_instance) delete _instance;
  _instance = new Logger(ident,logopt,facility,TZ);
  return _instance;
}

/* static */
Logger* Logger::createInstance(FILE* out) {
  if (_instance) delete _instance;
  _instance = new Logger(out);
  return _instance;
}
                                                                                
/* static */
Logger* Logger::getInstance() {
  if (!_instance) createInstance(stderr);
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

#if defined(SVR4) || ( defined(__GNUC__) && __GNUC__ > 1 )
void Logger::log(int severity, const char *fmt, ...)
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
  const char *fmt;
#endif

#if defined(SVR4) || ( defined(__GNUC__) && __GNUC__ > 1 )
  va_start(args,fmt);
#else
  va_start(args);
  severity = va_arg(args,int);
  fmt = va_arg(args,const char *);
#endif

  if (syslogit) {
    if (loggerTZ) {
      putenv(loggerTZ);
      tzset();
    }
    vsyslog(severity,fmt,args);
    if (loggerTZ) {
      putenv(saveTZ);
      tzset();
    }
  }
  else {
    int err = errno;	// grab it in case it changes!
    const char *p1,*p2;
    std::string newfmt;

    /*
     * Replace %m with strerror(errno), like syslog does,
     * then pass format to vfprintf.
     */
    for (p1=fmt; (p2 = strchr(p1,'%')); p1 = p2) {
      p2++;
      if (*p2 == 'm') {
	newfmt.append(p1,p2-1);
	newfmt.append(strerror(err));
	p2++;
      }
      else newfmt.append(p1,p2);
    }
    newfmt.append(p1);

    vfprintf(output,newfmt.c_str(),args);
    fprintf(output,"\n");
  }

  va_end(args);
}

