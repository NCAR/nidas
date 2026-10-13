#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/DSMSensor.h>

using namespace nidas::util;
using namespace nidas::core;

using nidas::core::Timeouts;

BOOST_AUTO_TEST_CASE(test_parse_timeouts)
{
    Timeouts timeouts;

    // set only the timeout
    BOOST_CHECK(timeouts.parse("1"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 1000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 0);

    // set both the reopen and power cycle timeouts
    BOOST_CHECK(timeouts.parse("1 2"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 1000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 2);

    // invalid input leaves timeouts unchanged
    BOOST_CHECK(!timeouts.parse("abc"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 1000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 2);

    // set both timeouts as floats and ensure millisecond resolution
    BOOST_CHECK(timeouts.parse("10.5 20"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 10500);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 20);

    // whatever parses is modified, even if parsing fails.
    timeouts = Timeouts{};
    BOOST_CHECK(!timeouts.parse("1.5 abc"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 1500);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 0);

    // additional fields are ignored
    BOOST_CHECK(timeouts.parse("4 3 2 1"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 4000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 3);

    // check behavior when a number is not followed by space or eof.
    // this may not be ideal to allow, but that's how the parsing works
    BOOST_CHECK(timeouts.parse("6 5x"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 6000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 5);

    // this will fail because there is more string to parse but it cannot be
    // parsed as the second timeout, but the second should not change.
    BOOST_CHECK(!timeouts.parse("7x"));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 7000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 5);

    // trailing spaces are ignored
    BOOST_CHECK(timeouts.parse("7 "));
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 7000);
    BOOST_CHECK_EQUAL(timeouts.getCycleNumTimeouts(), 5);
}
