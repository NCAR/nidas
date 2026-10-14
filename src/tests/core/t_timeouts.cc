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
    { "5 , 1 , 2   1 , 0 ", true, Timeouts(5000, 1000, 2).setPowerCycle(1) },
    // any amount of whitespace before a comma must not shift fields
    { "5  ,2", true, Timeouts(5000, 2000) },
    { "5 \t, 2  ,\t1.5   3  ,  0.5", true,
      Timeouts(5000, 2000, 1.5).setPowerCycle(3, 500) },
    { "5,2  ", true, Timeouts(5000, 2000) },
    // fractional power cycle count is an error, and the rest of the power
    // cycle group is not parsed
    { "10 2.5,5", false, Timeouts(10000) },
    // but a whole number written as a float is fine
    { "10 3.0,5", true, Timeouts(10000).setPowerCycle(3, 5000) },
    // negative timeout is forced to 0
    { "-5", false, Timeouts() },
    // maxdelay must be within the same limits as delay
    { "5,1,1,0.5", false, Timeouts(5000, 1000, 1, 1000) },
    { "5,1,1,90", false, Timeouts(5000, 1000, 1, 60000) }
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

    BOOST_CHECK_EQUAL(Timeouts::DELAY_MSECS_DEFAULT, 1000);
    BOOST_CHECK_EQUAL(Timeouts::MINDELAY_LIMIT_MSECS, 1000);
    BOOST_CHECK_EQUAL(Timeouts::MAXDELAY_LIMIT_MSECS, 60000);

    // whatever parses is modified, even if parsing fails.
    Timeouts timeouts;
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
    BOOST_CHECK_EQUAL(timeouts.getTotalNumPowerCycles(), 0);
    BOOST_CHECK_EQUAL(timeouts.getNumTimeouts(), 0);
    BOOST_CHECK(!timeouts.cyclePowerThisTimeout());

    // increment timeouts and check power cycle behavior
    timeouts.setPowerCycle(3, 500);
    BOOST_CHECK_EQUAL(timeouts.getWarmupMsecs(), 500);
    for (int i = 1; i <= 6; ++i) {
        timeouts.incrementTimeouts();
        BOOST_CHECK_EQUAL(timeouts.getNumTimeouts(), i);
        BOOST_CHECK_EQUAL(timeouts.cyclePowerThisTimeout(), !(i % 3));
    }

    BOOST_CHECK_EQUAL(timeouts.getTotalNumPowerCycles(), 2);
}


BOOST_AUTO_TEST_CASE(test_timeouts_delay)
{
    Timeouts timeouts(1000, 200, 1.5, 5000);

    // no timeouts yet means zero delay
    BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), 0);

    // initial delay (after the first timeout)can never be less than the
    // minimum
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(),
                      Timeouts::MINDELAY_LIMIT_MSECS);

    timeouts = Timeouts(1000, 2000, 1.5, 6000);
    for (auto xdelay: std::vector<int>{2000, 5000, 6000, 6000}) {
        timeouts.incrementTimeouts();
        BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), xdelay);
        // also just make extra sure power cycles are never enabled on any
        // timeouts
        BOOST_CHECK_EQUAL(timeouts.cyclePowerThisTimeout(), false);
    }

    timeouts = Timeouts(1000, 2000, .5, 5000);
    for (auto xdelay: std::vector<int>{2000, 3000, 4000, 5000, 5000}) {
        timeouts.incrementTimeouts();
        BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), xdelay);
    }

    // default backoff rate of 1.0 and default maximum delay
    timeouts = Timeouts(1000, 15000);
    for (auto xdelay: std::vector<int>{15000, 30000, 45000, 60000, 60000}) {
        timeouts.incrementTimeouts();
        BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), xdelay);
    }
}


BOOST_AUTO_TEST_CASE(test_timeouts_reset)
{
    Timeouts timeouts(1000);
    timeouts.setPowerCycle(2);

    // increment timeouts a few times
    for (int i = 1; i <= 5; ++i) {
        timeouts.incrementTimeouts();
    }
    BOOST_CHECK_EQUAL(timeouts.getNumTimeouts(), 5);
    BOOST_CHECK_EQUAL(timeouts.getTotalNumTimeouts(), 5);
    BOOST_CHECK_EQUAL(timeouts.getTotalNumPowerCycles(), 2);

    // reset the current timeout counter
    timeouts.resetTimeout();
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getNumTimeouts(), 1);
    BOOST_CHECK_EQUAL(timeouts.getTotalNumTimeouts(), 6);
    BOOST_CHECK_EQUAL(timeouts.getTotalNumPowerCycles(), 2);
}


BOOST_AUTO_TEST_CASE(test_timeouts_set_delay)
{
    Timeouts timeouts;

    // set a delay below the minimum limit
    timeouts.setDelayMsecs(0);
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), Timeouts::MINDELAY_LIMIT_MSECS);

    // set a delay above the maximum limit
    timeouts.setDelayMsecs(Timeouts::MAXDELAY_LIMIT_MSECS + 1000);
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), Timeouts::MAXDELAY_LIMIT_MSECS);

    // set a new delay without a timeout, as if this is a forced delay after a
    // reopen failure.
    timeouts.setDelayMsecs(3000);
    BOOST_CHECK_EQUAL(timeouts.getTimeoutMsecs(), 0);
    // before any timeouts, the adjusted delay should be zero
    BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), 0);
    timeouts.incrementTimeouts();
    BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), 3000);
}


BOOST_AUTO_TEST_CASE(test_timeouts_default_delays)
{
    // make sure the defaults are appropriate to slow down reopen attempts
    Timeouts timeouts;
    BOOST_CHECK_EQUAL(timeouts.getDelayMsecs(), Timeouts::DELAY_MSECS_DEFAULT);
    BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), 0);

    for (int i = 0; i < 5; ++i) {
        timeouts.incrementTimeouts();
        BOOST_CHECK_EQUAL(timeouts.getAdjustedDelayMsecs(), (i + 1) * 1000);
    }
}
