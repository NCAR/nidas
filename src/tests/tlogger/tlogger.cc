#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "nidas/util/Logger.h"
#include "nidas/util/Thread.h"
#include "math.h"
#include "sstream"
#include "errno.h"

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

  log->log (LOG_DEBUG, "%s", "test log message");
  BOOST_CHECK(regex_match(oss.str(), regex(".*DEBUG.*test log message\n")));
  oss.str("");

  // Now make sure a LogContext created after the scheme was set still
  // gets enabled.
  LogContext loghere(LOG_DEBUG);
  BOOST_CHECK (loghere.active());

  // Now make sure the log point above is enabled when it first gets
  // logged.
  log_things_here();
  BOOST_CHECK(regex_match(oss.str(), regex(".*DEBUG.*x = 42, e = 1\n")));
  oss.str("");
  
  // Now disable logging and check that nothing is logged.
  log->setScheme(LogScheme().clearConfigs());
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
  log->setScheme(LogScheme().addConfig (lc));
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
  log->setScheme(log->getScheme().addConfig (lc));
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

  LogScheme ts;
  ts.setShowFields ("level,time,function,message");
  BOOST_CHECK_EQUAL (ts.getShowFieldsString(), "level,time,function,message");

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
  log->updateScheme (replacement);
  BOOST_CHECK_EQUAL(log->getScheme().getShowFieldsString(), "all");
}



BOOST_AUTO_TEST_CASE(test_scheme_names)
{
  LogScheme scheme("");
  BOOST_CHECK (scheme.getName().length() > 0);
  scheme.setName("");
  BOOST_CHECK (scheme.getName().length() > 0);
  scheme = LogScheme();
  BOOST_CHECK (scheme.getName().length() > 0);
  scheme.setName("orange");
  BOOST_CHECK_EQUAL (scheme.getName(), "orange");

  // If we set the scheme with a non-existent name, the name
  // had still better be preserved.
  Logger* log = Logger::createInstance(&oss);
  log->setScheme("boo");
  BOOST_CHECK_EQUAL (log->getScheme().getName(), "boo");
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
  run() throw(Exception)
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
