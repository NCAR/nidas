
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/Sample.h>

using namespace nidas::core;


BOOST_AUTO_TEST_CASE(test_sample_type)
{

  SampleT<float> fsamp;
  BOOST_CHECK_EQUAL(fsamp.getType(), FLOAT_ST);

  SampleT<char> csamp;
  BOOST_CHECK_EQUAL(csamp.getType(), CHAR_ST);

  short* hptr = 0;
  BOOST_CHECK_EQUAL(getSampleType(hptr), SHORT_ST);
}

