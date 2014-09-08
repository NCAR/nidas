#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "NidasApp.h"

using namespace boost;
using namespace nidas::util;
using namespace nidas::core;

BOOST_AUTO_TEST_CASE(test_sample_match_all)
{
  SampleMatcher sm;

  sm.addCriteria("-1,-1");

  dsm_sample_id_t id = 0;
  for (int dsm = 1; dsm < 10; ++dsm)
  {
    for (int sid = 100; sid < 200; ++sid)
    {
      id = SET_DSM_ID(id, dsm);
      id = SET_SHORT_ID(id, sid);
      BOOST_CHECK_EQUAL(sm.match(id), true);
    }
  }
}


BOOST_AUTO_TEST_CASE(test_nidas_app_xml)
{
  NidasApp app;

  app.setArguments(NidasApp::XmlHeaderArgument |
		   NidasApp::LogLevelArgument);

  const char* argv[] = { "app", "-x", "xmlfile", "-l", "debug" };
  int argc = sizeof(argv)/sizeof(argv[0]);

  app.parseArguments(argv, argc);
  BOOST_CHECK_EQUAL(app.xmlHeaderFile(), "xmlfile");
  BOOST_CHECK_EQUAL(app.logLevel(), LOGGER_DEBUG);

}
