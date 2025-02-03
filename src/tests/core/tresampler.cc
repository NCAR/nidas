
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/Sample.h>
#include <nidas/core/NearestResamplerAtRate.h>


using namespace nidas::core;


BOOST_AUTO_TEST_CASE(test_resampler)
{
    std::vector<const Variable*> vars;

    NearestResamplerAtRate resampler(vars, true);
    resampler.setRate(1.0);
}

