/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "nidas/util/Logger.h"
#include "nidas/util/Thread.h"
#include "nidas/util/UTime.h"
#include "math.h"
#include "sstream"
#include "errno.h"

#include <cstdlib>

using namespace boost;
using namespace nidas::util;

struct LogObject
{

  LogObject()
  {
    DLOG(("in the constructor"));
  }

  void
  process_something()
  {
    ILOG(("started processing something"));
  }

  void
  finish_something()
  {
    ILOG(("finished processing something"));
  }

  ~LogObject()
  {
    DLOG(("being destroyed"));
  }

};




void
log_things_here()
{
  // Here's a log point for this function.
  int x = 42;
  DLOG(("x = %i, e = ", x) << 1);
}

void
houston()
{
  ELOG(("Houston, we have a problem."));
}

void
log_things_there()
{
  houston();
  PLOG(("Going, going, gone."));
}


static std::ostringstream oss;

using nidas::util::LogConfig;
using nidas::util::Logger;

BOOST_AUTO_TEST_CASE(test_log_format)
{
  errno = 1;
  BOOST_CHECK_EQUAL(LogMessage().format("%s: xx%m--",
          "fake error").getMessage(),
        "fake error: xxOperation not permitted--");

  errno = 1;
  // Now test stream output operator.
  LogMessage lf;
  lf.format("%s: xx%m", "fake error") << "-*-" << 42;
  BOOST_CHECK_EQUAL(lf.getMessage(), 
        "fake error: xxOperation not permitted-*-42");

  BOOST_CHECK_EQUAL((LogMessage().format("no parms, ") 
         << "more parms").getMessage(),
        "no parms, more parms");

  errno = 0;
}


BOOST_AUTO_TEST_CASE(test_logger)
{
  // Enable all messages by creating an all-inclusive LogConfig.
  LogConfig lc;
  oss.str("");

  // Create a Logger instance which will log to our string stream.
  Logger* log = Logger::createInstance(&oss);
  log->setScheme(LogScheme().addConfig(lc));

  log->log(LOG_DEBUG, "%s", "test log message");
  BOOST_CHECK(regex_match(oss.str(), regex(".*DEBUG.*test log message\n")));
  oss.str("");

  // Now make sure a LogContext created after the scheme was set still
  // gets enabled.
  LogContext loghere(LOG_DEBUG);
  BOOST_CHECK(loghere.active());

  // Now make sure the log point above is enabled when it first gets
  // logged.
  log_things_here();
  BOOST_CHECK(regex_match(oss.str(), regex(".*DEBUG.*x = 42, e = 1\n")));
  oss.str("");
  
  // Disable logging with an empty scheme and check that nothing is logged.
  log->setScheme(LogScheme());
  log_things_here();
  BOOST_CHECK_EQUAL(oss.str(), "");
  oss.str("");

  // Re-enable again and check that the existing registry of the log
  // point is activated again.
  log->setScheme(LogScheme().addConfig(lc));
  log_things_here();
  BOOST_CHECK(regex_match(oss.str(), regex(".*DEBUG.*x = 42, e = 1\n")));
  oss.str("");

  // Test that we can select one message but not another by function name.
  lc.function_match = "log_things_there";
  log->setScheme(LogScheme().addConfig (lc));
  log_things_here();
  log_things_there();
  BOOST_CHECK(regex_match(oss.str(), regex(".*ERROR.*Going, going, gone.\n")));
  oss.str("");

  // Enable all problems too.
  lc = LogConfig();
  lc.level = nidas::util::LOGGER_EMERG;
  LogScheme scheme = log->getScheme();
  scheme.addConfig(lc);
  log->setScheme(scheme);
  log_things_here();
  log_things_there();
  BOOST_CHECK(regex_match(oss.str(), regex(".*Houston, we have a problem.\n"
             ".*Going, going, gone.\n")));

  oss.str("");
  static LogContext lp(LOG_INFO);
  lc.level = nidas::util::LOGGER_INFO;
  log->setScheme(scheme.addConfig(lc));
  if (lp.active())
  {
    LogMessage msg;
    msg << "complicated info output...";
    lp.log(msg);
  }
  BOOST_CHECK(regex_match(oss.str(), 
        regex(".*INFO.*complicated info output...\n")));
}


BOOST_AUTO_TEST_CASE(test_default_log_scheme)
{
  // Make sure that an application will get a default logging scheme with
  // level WARNING if nothing else is set.
  Logger::clearSchemes();
  // The default scheme is always initialized and set as the current scheme.
  BOOST_CHECK(Logger::knownScheme(""));
  LogScheme ls = Logger::getScheme();
  BOOST_CHECK_EQUAL(ls.logLevel(), LOGGER_WARNING);
  BOOST_CHECK_EQUAL(ls.getName(), "");
  BOOST_CHECK(! Logger::knownScheme("default"));
}


BOOST_AUTO_TEST_CASE(test_log_scheme_fallback)
{
  // Add two configs as fallbacks, and make sure they get replaced.
  LogScheme ls = LogScheme().addFallback("info");
  LogScheme::log_configs_v configs = ls.getConfigs();
  BOOST_CHECK_EQUAL(configs.size(), 1);
  BOOST_CHECK_EQUAL(configs[0].level, LOGGER_INFO);
  ls.addFallback("debug");
  configs = ls.getConfigs();
  BOOST_CHECK_EQUAL(configs.size(), 2);
  BOOST_CHECK_EQUAL(configs[1].level, LOGGER_DEBUG);
  ls.addConfig("warning");
  configs = ls.getConfigs();
  BOOST_CHECK_EQUAL(configs.size(), 1);
  BOOST_CHECK_EQUAL(configs[0].level, LOGGER_WARNING);
}


BOOST_AUTO_TEST_CASE(test_object_logging)
{
  LogConfig lc;
  Logger* log = Logger::createInstance(&oss);
  log->setScheme(LogScheme());
  oss.str("");

  // Check that we can select log messages by object class.
  {
    LogObject lo;
    lo.process_something();
    lo.finish_something();
  }
  BOOST_CHECK_EQUAL(oss.str(), "");
  oss.str("");
  lc.function_match = "LogObject::";
  log->setScheme(LogScheme().addConfig(lc));
  {
    LogObject lo;
    lo.process_something();
    lo.finish_something();
  }
  BOOST_CHECK(regex_match(oss.str(), regex(".*in the constructor\n"
              ".*started processing something\n"
              ".*finished processing something\n"
              ".*being destroyed\n")));

  // Now test that we can disable a log message.
  oss.str("");
  lc.function_match = "LogObject::finish_";
  lc.activate = false;
  log->setScheme(log->getScheme().addConfig(lc));
  {
    LogObject lo;
    lo.process_something();
    lo.finish_something();
  }
  BOOST_CHECK(!regex_match(oss.str(), regex(".*in the constructor\n"
              ".*started processing something\n"
              ".*finished processing something\n"
              ".*being destroyed\n")));
  BOOST_CHECK(regex_match(oss.str(), regex(".*in the constructor\n"
             ".*started processing something\n"
             ".*being destroyed\n")));
}


BOOST_AUTO_TEST_CASE(test_level_strings)
{
  BOOST_CHECK_EQUAL(logLevelToString(LOGGER_EMERGENCY),"emergency");
  BOOST_CHECK_EQUAL(logLevelToString(LOGGER_DEBUG),"debug");
  BOOST_CHECK_EQUAL(logLevelToString(LOGGER_INFO),"info");
  BOOST_CHECK_EQUAL(logLevelToString(LOGGER_WARNING),"warning");

  BOOST_CHECK_EQUAL(stringToLogLevel("warn"),LOGGER_WARNING);
  BOOST_CHECK_EQUAL(stringToLogLevel("info"),LOGGER_INFO);
  BOOST_CHECK_EQUAL(stringToLogLevel("INFO"),LOGGER_INFO);
  BOOST_CHECK_EQUAL(stringToLogLevel("problem"),LOGGER_ERROR);
  BOOST_CHECK_EQUAL(stringToLogLevel("error"),LOGGER_ERROR);
  BOOST_CHECK_EQUAL(stringToLogLevel("debug"),LOGGER_DEBUG);
  BOOST_CHECK_EQUAL(stringToLogLevel("xdebug"),-1);
  BOOST_CHECK_EQUAL(stringToLogLevel("unknown"),-1);
}


BOOST_AUTO_TEST_CASE(test_log_fields)
{
  BOOST_CHECK_EQUAL(LogScheme::stringToField("thread"),LogScheme::ThreadField);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("time"),LogScheme::TimeField);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("level"),LogScheme::LevelField);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("function"),
        LogScheme::FunctionField);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("file"),LogScheme::FileField);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("message"),
        LogScheme::MessageField);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("all"), LogScheme::AllFields);
  BOOST_CHECK_EQUAL(LogScheme::stringToField("x"), LogScheme::NoneField);

  LogScheme ls;
  BOOST_CHECK_THROW(ls.setShowFields("level,x"), std::runtime_error);
  BOOST_CHECK_THROW(ls.setShowFields("x"), std::runtime_error);
  BOOST_CHECK_THROW(ls.setShowFields("x,level"), std::runtime_error);
  // field is file not filename
  BOOST_CHECK_THROW(ls.setShowFields("level,filename"), std::runtime_error);
  BOOST_CHECK_THROW(ls.setShowFields("level,fn"), std::runtime_error);

  LogScheme ts;
  ts.setShowFields("level,time,function,message");
  BOOST_CHECK_EQUAL(ts.getShowFieldsString(), "level,time,function,message");

  LogConfig lc;
  Logger* log = Logger::createInstance(&oss);
  log->setScheme(LogScheme().addConfig(lc));

  // Verify that we can change the output message fields.
  oss.str("");
  log->setScheme(log->getScheme().setShowFields("message,level"));
  houston();
  BOOST_CHECK_EQUAL(oss.str(), "Houston, we have a problem.|EMERGENCY\n");
  oss.str("");
  log->setScheme(log->getScheme().setShowFields("function"));
  houston();
  BOOST_CHECK_EQUAL(oss.str(), "void houston()\n");
  oss.str("");
  log->setScheme(log->getScheme().setShowFields("level,message"));
  houston();
  BOOST_CHECK_EQUAL(oss.str(), "EMERGENCY|Houston, we have a problem.\n");
  oss.str("");
  // This actually sets no fields to be shown.  Not sure if this is really an
  // intuitive result, but that is the current behavior...
  log->setScheme(log->getScheme().setShowFields(""));
  houston();
  BOOST_CHECK_EQUAL(oss.str(), "");

  // Verify that we can set a scheme in the logger, and then update it
  // by updating a scheme with the same name.
  log->setScheme(LogScheme("testing").setShowFields("file,function"));
  LogScheme replacement("testing");
  replacement.setShowFields("all");
  // Changing the replacement of course should change nothing.
  BOOST_CHECK_EQUAL(log->getScheme().getShowFieldsString(), "file,function");
  // Adding it back makes the change.
  log->updateScheme(replacement);
  BOOST_CHECK_EQUAL(log->getScheme().getShowFieldsString(), "all");
}


BOOST_AUTO_TEST_CASE(test_scheme_names)
{
  // It used to be that scheme names could not be empty...
  LogScheme scheme;
  BOOST_CHECK (scheme.getName().length() == 0);
  scheme.setName("");
  BOOST_CHECK (scheme.getName().length() == 0);
  scheme.setName("orange");
  BOOST_CHECK_EQUAL(scheme.getName(), "orange");

  // If we set the scheme with a non-existent name, the name
  // had better be preserved.
  Logger::setScheme("boo");
  BOOST_CHECK_EQUAL(Logger::getScheme().getName(), "boo");
  // Likewise fetching a non-existent scheme returns 
  // the default scheme with log level warning, but does not
  // change the active scheme.
  Logger::clearSchemes();
  BOOST_CHECK_EQUAL(Logger::getScheme("boohoo").logLevel(), LOGGER_WARNING);
  BOOST_CHECK_EQUAL(Logger::getScheme().getName(), "");
  BOOST_CHECK(Logger::knownScheme("boohoo"));
  BOOST_CHECK(! Logger::knownScheme("boo"));
  // Checking for a scheme does not create it:
  BOOST_CHECK(! Logger::knownScheme("boo"));
}


BOOST_AUTO_TEST_CASE(test_scheme_parameters)
{
  LogScheme scheme("");

  BOOST_CHECK_EQUAL(scheme.getParameter("x", "y"), "y");
  BOOST_CHECK_EQUAL(scheme.getParameter("x"), "");
  scheme.setParameter("variables", "x,y,z");
  BOOST_CHECK_EQUAL(scheme.getParameter("variables"), "x,y,z");
  ::setenv("x", "ray", true);
  BOOST_CHECK_EQUAL(scheme.getEnvParameter("x"), "ray");
  BOOST_CHECK_EQUAL(scheme.getParameter("x"), "");
  scheme.setParameter("x", "men");
  BOOST_CHECK_EQUAL(scheme.getEnvParameter("x"), "men");

  BOOST_CHECK_EQUAL(scheme.getParameterT("limit", 1000), 1000);
  scheme.setParameter("limit", "999");
  BOOST_CHECK_EQUAL(scheme.getParameterT("limit", 1000), 999);
}


BOOST_AUTO_TEST_CASE(test_logconfig_parse)
{
  LogConfig lc;

  BOOST_CHECK_EQUAL(lc.level, LOGGER_DEBUG);
  BOOST_CHECK_EQUAL(lc.filename_match, "");

  lc.parse("");
  lc.parse("info");
  BOOST_CHECK_EQUAL(lc.level, LOGGER_INFO);

  lc.parse("file=dynld");
  BOOST_CHECK_EQUAL(lc.level, LOGGER_INFO);
  BOOST_CHECK_EQUAL(lc.filename_match, "dynld");

  lc.parse("line=99,tag=slices,level=verbose");
  BOOST_CHECK_EQUAL(lc.line, 99);
  BOOST_CHECK_EQUAL(lc.tag_match, "slices");
  BOOST_CHECK_EQUAL(lc.level, LOGGER_VERBOSE);

  BOOST_CHECK_THROW(lc.parse("x"), std::runtime_error);
  BOOST_CHECK_THROW(lc.parse("x=y"), std::runtime_error);
  BOOST_CHECK_THROW(lc.parse("line=-1"), std::runtime_error);
  BOOST_CHECK_THROW(lc.parse("line=0"), std::runtime_error);
}



/**
 * Run a function object in its own thread.
 **/
template <typename FO>
class ThreadFunction : public Thread
{
public:

  ThreadFunction(const std::string& name, FO& function) :
    Thread(name), _function(function)
  {}

  virtual
  ~ThreadFunction()
  {}

  virtual int
  run()
  {
    return _function();
  }

private:
  FO& _function;

};

template <typename FO>
Thread*
make_thread(const std::string&name, FO& function)
{
  return new ThreadFunction<FO>(name, function);
}


int
logging_function()
{
  // Create a series of log points, and as long as they are thread-local,
  // an entry should end up in the registry for each thread.  After the
  // threads exit, only the automatic log point should have been removed,
  // since the others are static.

  ILOGT("start", ("LogThread: ") << "starting run() method");

  static LogContext lp(LOG_INFO, "middle");

  ILOGT("end", ("LogThread: ") << "ending run() method");

  return 0;
}


BOOST_AUTO_TEST_CASE(test_multithread)
{
  LogScheme ts;
  ts.setShowFields ("level,time,function,message,thread");

  LogConfig lc;
  lc.level = LOGGER_DEBUG;
  ts.addConfig(lc);
  Logger::getInstance()->setScheme(ts);

  std::vector<Thread*> threads;

  for (int i = 0; i < 5; ++i)
  {
    std::ostringstream name;
    name << "thread" << i;
    Thread* thread = make_thread(name.str(), logging_function);
    threads.push_back(thread);
    thread->start();
  }

  for (unsigned int i = 0; i < threads.size(); ++i)
  {
    threads[i]->join();
    delete threads[i];
  }

}

std::string
get_ident(const std::string& text, int n)
{
  if (n > 0)
    return get_ident(text + ".", n-1);
  return text;
}

BOOST_AUTO_TEST_CASE(test_syslog_cmd)
{
  // Create a syslog logger and make sure the name sticks.
  Logger::clearSchemes();
  Logger* logger;
  logger = Logger::createInstance(get_ident("test_cmd", 10).c_str(),
                                  LOG_CONS, LOG_LOCAL5);
  // For some reason, when running this test under valgrind, the log messages
  // sometimes are not reported in /var/log/messages or with journalctl -f.  The
  // messages are all different to make sure the problem is not some kind of
  // duplicate suppression.  Either all the messages are reported or none of
  // them.  There are no error messages on the console from syslog saying the
  // messages could not be logged, so I don't know where they go... The messages
  // all appear in the system logs when not running under valgrind.
  UTime ut;
  for (int ntest = 1; ntest <= 5; ++ntest)
  {
    ut += USECS_PER_SEC;
    logger->log(LOG_ERR, "test #%d @ %s", ntest, ut.format().c_str());
  }
  // Sanity check on destroyInstance().  When this code was written to use a
  // public destructor, there were memory errors.  Somehow logger->_ident was
  // being accessed by syslog after the Logger had been destroyed and _ident
  // deleted.  That led to the realization that the destructor was not
  // protecting access to the _instance pointer like all the createInstance()
  // methods, so the destructor is no longer public.  Now the Logger instance
  // can be destroyed without valgrind reporting errors about _ident access.
  BOOST_CHECK_EQUAL(logger, Logger::getInstance());
  Logger::destroyInstance();
  // Turns out this is not so helpful a check, because the next Logger instance
  // is likely to be allocated in the same place as the previous.  Oh well.
  // BOOST_CHECK_NE(logger, Logger::getInstance());
}


BOOST_AUTO_TEST_CASE(test_logpoints_no_deadlock)
{
  // Make sure show log points does not deadlock if a Logger instance has not
  // been created yet.
  LogContext loghere(LOG_DEBUG);
  Logger::destroyInstance();
  // This should create a default logger instance without deadlocking.
  LogScheme().showLogPoints(true);
}

