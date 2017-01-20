#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "Timetable.h"
#include "nidas/util/UTime.h"

#include <sys/types.h>
#include <signal.h>
#include <iostream>

using namespace nidas::util;
using namespace nidas::core;



BOOST_AUTO_TEST_CASE(test_default_timetable_time)
{
  TimetablePeriod ss(TimetablePeriod::ON,
		     TimetableTime("2017-12-01,00:00:00").getStartTime());

  BOOST_CHECK_EQUAL(ss.getTag(), TimetablePeriod::ON);
  BOOST_CHECK_EQUAL("on", TimetablePeriod::ON);
  BOOST_CHECK_EQUAL(TimetablePeriod().getTag(), TimetablePeriod::DEFAULT);

  UTime ut = UTime::parse(true, "1900 01 01 00:00:00");
  TimetableTime glob;
  glob.parse("*-*-*,*:*:*");
  BOOST_CHECK_EQUAL(ut, glob.getStartTime());
}


BOOST_AUTO_TEST_CASE(test_time_parse)
{
  TimetableTime stime;

  stime.parse("2017-12-01,01:02:30");
  BOOST_CHECK_EQUAL(stime.year, 2017);
  BOOST_CHECK_EQUAL(stime.month, 12);
  BOOST_CHECK_EQUAL(stime.day, 1);
  BOOST_CHECK_EQUAL(stime.hour, 1);
  BOOST_CHECK_EQUAL(stime.minute, 2);
  BOOST_CHECK_EQUAL(stime.second, 30);

  UTime ut = UTime::parse(true, "2017 12 01 01:02:30");

  TimetableTime fixed;
  fixed.setFixedTime(ut);
  std::cerr << "UTime=" << ut << ", fixed=";
  fixed.toStream(std::cerr);
  std::cerr << std::endl;
  BOOST_CHECK_EQUAL(fixed.year, 2017);
  BOOST_CHECK_EQUAL(fixed.month, 12);
  BOOST_CHECK_EQUAL(fixed.day, 1);
  BOOST_CHECK_EQUAL(fixed.hour, 1);
  BOOST_CHECK_EQUAL(fixed.minute, 2);
  BOOST_CHECK_EQUAL(fixed.second, 30);

  {
    std::ostringstream outs;
    fixed.toStream(outs);
    BOOST_CHECK_EQUAL(outs.str(), "2017-12-01,01:02:30");
  }

  BOOST_CHECK_EQUAL(stime.match(ut), true);
    
  ut = UTime::parse(true, "2017 12 02 01:02:30");
  BOOST_CHECK_EQUAL(stime.match(ut), false);
    
  stime.day = TimetableTime::ANYTIME;
  ut = UTime::parse(true, "2017 12 02 01:02:30");
  BOOST_CHECK_EQUAL(stime.match(ut), true);

  {
    std::ostringstream outs;
    stime.toStream(outs);
    BOOST_CHECK_EQUAL(outs.str(), "2017-12-*,01:02:30");
  }

  stime.parse("*-*-*,*:30:00");
  BOOST_CHECK_EQUAL(stime.year, TimetableTime::ANYTIME);
  BOOST_CHECK_EQUAL(stime.month, TimetableTime::ANYTIME);
  BOOST_CHECK_EQUAL(stime.day, TimetableTime::ANYTIME);
  BOOST_CHECK_EQUAL(stime.hour, TimetableTime::ANYTIME);
  BOOST_CHECK_EQUAL(stime.minute, 30);
  BOOST_CHECK_EQUAL(stime.second, 00);

}


BOOST_AUTO_TEST_CASE(test_decimal_parse)
{
  TimetableTime stime;

  stime.parse("2017-12-09,01:02:30");
  BOOST_CHECK_EQUAL(stime.year, 2017);
  BOOST_CHECK_EQUAL(stime.month, 12);
  BOOST_CHECK_EQUAL(stime.day, 9);
  BOOST_CHECK_EQUAL(stime.hour, 1);
  BOOST_CHECK_EQUAL(stime.minute, 2);
  BOOST_CHECK_EQUAL(stime.second, 30);
}

