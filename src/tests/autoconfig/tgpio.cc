
#include <nidas/util/GpioIF.h>

#include <boost/test/unit_test.hpp>

using namespace nidas::util;


BOOST_AUTO_TEST_CASE(test_gpio2str)
{
    BOOST_CHECK_EQUAL(gpio2Str(SER_PORT5), "GPIO_SER_PORT5");
    BOOST_CHECK_EQUAL(gpio2Str(WIFI_SW), "GPIO_WIFI_SW2");
    BOOST_CHECK_EQUAL(gpio2Str(DEFAULT_SW), "GPIO_DEFAULT_SW1");
}
