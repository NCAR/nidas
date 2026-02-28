#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

// Someday DataStats will be in the core library where it might be shared, but
// for now include it directly into a test harness.

#include <nidas/util/UTime.h>

namespace testing {
    nidas::util::UTime now(nidas::util::UTime::ZERO);
}

#define DATA_STATS_OMIT_MAIN
#define DATA_STATS_UTIME_NOW testing::now
#include <nidas/apps/data_stats.cc>

using nidas::util::UTime;

BOOST_AUTO_TEST_CASE(test_data_stats_restart)
{
    DataStats stats;

    // configure DataStats with command line arguments until someday there's a
    // way to do it with an API.
    vector<const char*> args = {
        "data_stats",
        "--log", "debug",
        "--period", "10",
        "--count", "3",
        "marshall_ttt_20250527_120000_30s.dat"
    };
    stats.parseRunstring(args.size(), const_cast<char**>(args.data()));

    // in order to test how DataStats handles different scenarios of receiving
    // real-time samples, we need to be able to spoof the system time. skip
    // the input setup and go straight to setting up the first window.
    stats.setRealtime(true);
    UTime start, end;

    testing::now = UTime(true, 2010, 1, 1, 0, 0, 0);
    stats.advanceStats();
    stats.getPeriod(start, end);

    BOOST_TEST(start == testing::now);
    BOOST_TEST(end == testing::now + 10 * USECS_PER_SEC);

    // next time the stats advance, even without receiving any samples, the
    // period should advance to the current system time
    testing::now = UTime(true, 2026, 2, 27, 12, 0, 0);
    stats.advanceStats();
    stats.getPeriod(start, end);

    BOOST_TEST(start == testing::now);
    BOOST_TEST(end == testing::now + 10 * USECS_PER_SEC);
}
