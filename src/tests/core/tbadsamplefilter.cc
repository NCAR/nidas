
#define BOOST_TEST_DYN_LINK
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/BadSampleFilter.h>

using std::isnan;
using namespace nidas::util;
using namespace nidas::core;


BOOST_AUTO_TEST_CASE(test_bad_sample_filter)
{
  BadSampleFilter bsf;

  BOOST_CHECK_EQUAL(bsf.filterBadSamples(), false);
  bsf.setFilterBadSamples(true);
  BOOST_CHECK_EQUAL(bsf.filterBadSamples(), true);

  bsf.setRules("off");
  BOOST_CHECK_EQUAL(bsf.filterBadSamples(), false);

  bsf.setRules("mindsm=10,maxdsm=20,minlen=5,maxlen=1024");
  BOOST_CHECK_EQUAL(bsf.minDsmId(), 10);
  BOOST_CHECK_EQUAL(bsf.maxDsmId(), 20);
  BOOST_CHECK_EQUAL(bsf.minSampleLength(), 5);
  BOOST_CHECK_EQUAL(bsf.maxSampleLength(), 1024);
  BOOST_CHECK_EQUAL(bsf.filterBadSamples(), true);

  bsf.setRules("mintime=2019-07-04 12:34:56.1");
  bsf.setRules("maxtime=2019-10-15 02:04:06.1");
  
  BOOST_CHECK_EQUAL(bsf.minDsmId(), 10);
  BOOST_CHECK_EQUAL(bsf.maxDsmId(), 20);
  BOOST_CHECK_EQUAL(bsf.minSampleLength(), 5);
  BOOST_CHECK_EQUAL(bsf.maxSampleLength(), 1024);
  BOOST_CHECK_EQUAL(bsf.minSampleTime(), UTime(true, 2019, 7, 4, 12, 34, 56.1));
  BOOST_CHECK_EQUAL(bsf.maxSampleTime(), UTime(true, 2019, 10, 15, 2, 4, 6.1));
}

