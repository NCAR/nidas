
#define BOOST_TEST_DYN_LINK
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/util/MutexCount.h>

using namespace nidas::util;


BOOST_AUTO_TEST_CASE(test_mutex_count)
{
  MutexCount<int> v;

  // initial value is 0 and int cast works
  BOOST_CHECK_EQUAL((int)v, 0);
  //  BOOST_CHECK_EQUAL((int)v++, 0);
  BOOST_CHECK_EQUAL((int)++v, 1);
  BOOST_CHECK_EQUAL((int)++v, 2);
  BOOST_CHECK_EQUAL(v.value(), 2);
  //  BOOST_CHECK_EQUAL((int)v--, 2);
  BOOST_CHECK_EQUAL((int)--v, 1);
  BOOST_CHECK_EQUAL((int)--v, 0);
}

