#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/DSMSensor.h>

using namespace nidas::util;
using namespace nidas::core;

using nidas::core::Timeouts;

struct timeouts_test
{
    std::string text;
    bool xresult;
    Timeouts xtimeouts;
};

std::vector<timeouts_test> timeouts_tests = {
    { "1", true, Timeouts(1000) },
    { "1 2", true, Timeouts(1000).setPowerCycle(2) },
    // extra fields allowed and ignored
    { "1 2 3", true, Timeouts(1000).setPowerCycle(2) },
    // first field is required but invalid
    { "abc", false, Timeouts() },
    { "1.5 abc", false, Timeouts(1500) },
    { "4 3 2 1", true, Timeouts(4000).setPowerCycle(3) },
    // partial parse but still valid because two fields parsed
    { "6 5x", true, Timeouts(6000).setPowerCycle(5) },
    // partial parse but invalid because subsequent text must be parsed as a
    // valid power cycle setting
    { "7x", false, Timeouts(7000) },
    // trailing spaces are ignored
    { "7 ", true, Timeouts(7000) },
    // sensor that retries reopens every second and never backs off
    { "1,1,0", true, Timeouts(1000, 1000, 0) },
    // nanobarometer example where it keeps trying to open without power
    // cycling and backs off slowly and never waits more than 10 seconds
    { "2,1,.2,10", true, Timeouts(2000, 1000, 0.2, 10000) },
    // invalid delay of 0, forced to 1000
    { "5,0", false, Timeouts(5000, 1000) },
    // TRH needs a power cycle every timeout, but backs off quickly in case it
    // isn't there.
    { "5,1,2 1,0", true, Timeouts(5000, 1000, 2).setPowerCycle(1) },
    // same as above but with excessive spaces
    { "5, 1, 2 1, 0", true, Timeouts(5000, 1000, 2).setPowerCycle(1) },
    { "5 , 1 , 2   1 , 0 ", true, Timeouts(5000, 1000, 2).setPowerCycle(1) }
};


BOOST_AUTO_TEST_CASE(test_parse_timeouts)
{
    for (const auto& test : timeouts_tests)
    {
        Timeouts timeouts;
        BOOST_TEST_MESSAGE("Parsing test: " << test.text);
        BOOST_CHECK_EQUAL(timeouts.parse(test.text), test.xresult);
        BOOST_CHECK_EQUAL(timeouts, test.xtimeouts);
    }

    Timeouts timeouts;


    // whatever parses is modified, even if parsing fails.
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


BOOST_AUTO_TEST_CASE(test_timeouts_power_cycle)
{
    Timeouts timeouts(1000);

    // initially no power cycles
    BOOST_CHECK_EQUAL(timeouts.getNumPowerCycles(), 0);
    BOOST_CHECK_EQUAL(timeouts.getNumTimeouts(), 0);
    BOOST_CHECK(!timeouts.cyclePowerThisTimeout());

    // increment timeouts and check power cycle behavior
    timeouts.setPowerCycle(3, 500);
    BOOST_CHECK_EQUAL(timeouts.getWarmupMsecs(), 500);
    for (int i = 1; i <= 6; ++i) {
        timeouts.incrementTimeouts();
        BOOST_CHECK_EQUAL(timeouts.getNumTimeouts(), i);
        if (i % 3 == 0) {
            BOOST_CHECK(timeouts.cyclePowerThisTimeout());
        } else {
            BOOST_CHECK(!timeouts.cyclePowerThisTimeout());
        }
    }

    BOOST_CHECK_EQUAL(timeouts.getNumPowerCycles(), 2);
}


BOOST_AUTO_TEST_CASE(test_timeouts_delay)
{
    Timeouts timeouts(1000, 200, 1.5, 5000);

    // initial delay can never be less than the minimum
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(),
                      Timeouts::MINDELAY_LIMIT_MSECS);

    timeouts = Timeouts(1000, 2000, 1.5, 6000);

    // after one timeout, delay should be the initial delay
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), 2000);

    // after two timeouts, delay should increase
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), 5000);

    // after three timeouts, delay should increase but not exceed maxdelay_msecs
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), 6000);

    // after more timeouts, delay should still be capped at maxdelay_msecs
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), 6000);
}
