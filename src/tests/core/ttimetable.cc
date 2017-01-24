#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include "Timetable.h"
#include "nidas/util/UTime.h"
#include "nidas/core/Project.h"
#include "nidas/core/XMLParser.h"
#include "nidas/core/DSMSensor.h"

#include <sys/types.h>
#include <signal.h>
#include <iostream>

#include <boost/regex.hpp>

using namespace nidas::util;
using namespace nidas::core;



BOOST_AUTO_TEST_CASE(test_default_timetable_time)
{
  TimetablePeriod ss(TimetablePeriod::ON,
		     TimetableTime("2017-12-01,00:00:00").getStartTime());

  BOOST_CHECK_EQUAL(ss.getTag(), TimetablePeriod::ON);
  BOOST_CHECK_EQUAL(ss.getStart(), UTime::parse(true, "2017 12 01 00:00:00"));
  BOOST_CHECK_EQUAL("on", TimetablePeriod::ON);
  BOOST_CHECK_EQUAL(TimetablePeriod().getTag(), TimetablePeriod::DEFAULT);

  UTime ut = UTime::parse(true, "1970 01 01 00:00:00");
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

  TimetableTime ctime("2017-12-01,01:02:30");
  BOOST_CHECK_EQUAL(ctime.year, 2017);
  BOOST_CHECK_EQUAL(ctime.month, 12);
  BOOST_CHECK_EQUAL(ctime.day, 1);
  BOOST_CHECK_EQUAL(ctime.hour, 1);
  BOOST_CHECK_EQUAL(ctime.minute, 2);
  BOOST_CHECK_EQUAL(ctime.second, 30);

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


BOOST_AUTO_TEST_CASE(test_period_resolve)
{
  UTime one(UTime::parse(true, "2017 12 09 01:02:30"));
  UTime two(UTime::parse(true, "2017 12 09 02:00:00"));
  UTime three(UTime::parse(true, "2017 12 09 03:02:30"));

  // This is a 30-minute period starting at time two.
  TimetablePeriod tp(TimetablePeriod::ON, two, 30*60);

  UTime begin;
  UTime end;
  tp.resolve(one, three, &begin, &end);
  BOOST_CHECK_EQUAL(begin, two);
  BOOST_CHECK_EQUAL(end, begin+UTMinutes(30));

  UTime dend = TimetablePeriod::DEFAULT_END;
  UTime dstart = TimetablePeriod::DEFAULT_START;
  
  // Now try a time period with only duration.
  tp = TimetablePeriod(TimetablePeriod::ON, 30*60);

  // If previous period has no end and this one has no start,
  // then it should get end of time.
  tp.resolve(dend, dstart, &begin, &end);
  BOOST_CHECK_EQUAL(begin, dend);
  BOOST_CHECK_EQUAL(end, dend);

  // If previous has an end, then this one ends relative to that one.
  tp.resolve(one, three, &begin, &end);
  BOOST_CHECK_EQUAL(begin, one);
  BOOST_CHECK_EQUAL(end, one + UTMinutes(30));

  // Start with no duration.
  tp = TimetablePeriod(TimetablePeriod::ON, three);
  tp.resolve(one, dend, &begin, &end);
  BOOST_CHECK_EQUAL(begin, three);
  BOOST_CHECK_EQUAL(end, dend);

}


BOOST_AUTO_TEST_CASE(test_timetable_lookup)
{
  UTime one(UTime::parse(true, "2017 12 09 01:02:30"));
  UTime two(UTime::parse(true, "2017 12 09 02:00:00"));
  UTime three(UTime::parse(true, "2017 12 09 03:02:30"));

  UTime when(UTime::parse(true, "2017 12 09 02:15:30"));

  UTime end = TimetablePeriod::DEFAULT_END;
  UTime start = TimetablePeriod::DEFAULT_START;
  
  // This is a 30-minute period starting at time two.
  TimetablePeriod tp(TimetablePeriod::ON, two, 30*60);

  BOOST_CHECK_EQUAL(tp.contains(one, three, when), true);
  BOOST_CHECK_EQUAL(tp.contains(one, end, when), true);
  BOOST_CHECK_EQUAL(tp.contains(start, end, when), true);

  // The previous does not have a fixed end time, so this period starts
  // when it starts and still contains when.
  BOOST_CHECK_EQUAL(tp.contains(end, start, when), true);

  // A 30-minute period with no start will not contain any times if the
  // previous does not end.
  TimetablePeriod nostart(TimetablePeriod::ON, 30*60);
  BOOST_CHECK_EQUAL(nostart.contains(end, start, when), false);


  when = UTime::parse(true, "2017 12 09 02:45:30");

  BOOST_CHECK_EQUAL(tp.contains(one, three, when), false);
  BOOST_CHECK_EQUAL(tp.contains(one, end, when), false);
  BOOST_CHECK_EQUAL(tp.contains(start, end, when), false);

}


BOOST_AUTO_TEST_CASE(test_xml_parse)
{
  // Parse a project XML file and verify the
  // timetable parts.

  xercesc::DOMDocument* doc = parseXMLConfigFile("timetable_test.xml");
  Project* project;
  project = Project::getInstance();
  project->fromDOMElement(doc->getDocumentElement());

  dsm_sample_id_t sid = 0;
  sid = SET_SPS_ID(sid, 200);
  sid = SET_DSM_ID(sid, 105);
  DSMSensor* sensor = project->findSensor(sid);

  // 200 has the timetable which is on after April 1.
  TimetableTime ttime("2017-05-04,00:00:00");
  UTime when = ttime.getStartTime();
  BOOST_CHECK_EQUAL(when, UTime::parse(true, "2017 05 04 00:00:00"));
  BOOST_CHECK_EQUAL(sensor->getTimetableTag(when), "on");
  
  // 20 has no timetable.
  sid = SET_SPS_ID(sid, 20);
  sid = SET_DSM_ID(sid, 105);
  sensor = project->findSensor(sid);
  BOOST_CHECK_EQUAL(sensor->getTimetableTag(when), "");
}


