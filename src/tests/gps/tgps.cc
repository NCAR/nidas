// -*- c-basic-offset: 4; -*-
#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/Project.h>
#include <nidas/dynld/GPS_NMEA_Serial.h>
#include <nidas/util/UTime.h>
#include <cmath> // isnan

using namespace nidas::util;
using namespace nidas::core;
using namespace nidas::dynld;

using std::string;
using std::vector;

struct trec_t
{
    const char* input;
    const char* stime;
    bool ok;
};

struct trec_t trecs[] = {
    { "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A*6E\r\n",
      "2019 12 12 22:00:09.0000", true },
    { "$GPGGA,044043.00,4012.55682,N,08824.39657,W,2,12,03,224.6,M,-33.2,M,,"
      "0000*6C\r\n",
      "2018 10 01 04:40:43.0000", true },
    { "$GPRMC144634.00,A,4012.55779,N,08824.3962,W,0.010,,041018,,,D*6A\r\n",
      "2018 10 04 14:46:34.0000", false }
};

#if BOOST_VERSION <= 105300
BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(test_gps_time_rejects, 1);
#endif

BOOST_AUTO_TEST_CASE(test_gps_time_rejects)
{
#if BOOST_VERSION <= 105300
    BOOST_CHECK_MESSAGE(false, "boost test version is too old");
#else
    Logger* logger = Logger::createInstance(&std::cerr);
    logger->setScheme(LogScheme().addConfig(LogConfig()));

    GPS_NMEA_Serial* gps = new GPS_NMEA_Serial();

    const char* ckmsg = trecs[0].input;
    char cksum;
    bool gotck = gps->findChecksum(cksum, ckmsg, strlen(ckmsg));
    BOOST_TEST(gotck);
    BOOST_TEST(cksum == 0x6e);
    BOOST_TEST(gps->checksumOK(ckmsg, strlen(ckmsg)));

    vector<double> d(10);

    for (unsigned int i = 0; i < sizeof(trecs) / sizeof(trecs[0]); ++i)
    {
        BOOST_TEST_MESSAGE(">>> running test data #" << i << "...");
        trec_t& tc = trecs[i];

        dsm_time_t texp =
            UTime::parse(true, tc.stime, "%Y %m %d %H:%M:%S.%4f").toUsecs();
        dsm_time_t traw = texp + USECS_PER_SEC / 2;
        dsm_time_t tt;
        if (tc.input[3] == 'G')
            tt = gps->parseGGA(tc.input + 7, &(d[0]), d.size(), traw);
        else
            tt = gps->parseRMC(tc.input + 7, &(d[0]), d.size(), traw);
        string tgot = UTime(tt).format(true, "%Y %m %d %H:%M:%S.%4f");
        string sraw = UTime(traw).format(true, "%Y %m %d %H:%M:%S.%4f");
        BOOST_TEST_MESSAGE("expected " << tc.stime << ", parsed time "
                                       << tgot);
        BOOST_TEST((tc.ok ? tc.stime : sraw) == tgot);

#ifdef notdef
        // There's no point to this until we can fake
        // GPS_NMEA_Serial::process() into processing messages without a
        // corresponding tag id, ie, _rmcId needs to be non-zero.

        // Process a raw sample.  It should succeed, and the new sample time
        // should match the parsed time instead of the raw time.
        SampleT<char>* sample = getSample<char>(strlen(tc.input) + 1);
        strcpy(sample->getDataPtr(), tc.input);
        sample->setTimeTag(traw);

        std::list<const Sample*> results;
        bool result = gps->process(sample, results);
        BOOST_CHECK_EQUAL(tc.ok, result);
        if (result)
        {
            BOOST_CHECK_EQUAL(results.front()->getTimeTag(), texp);
        }
        sample->freeReference();
#endif
    }

    const char* goodmsg =
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A*6E";

    // Make sure we can add checksum and things still work.
    char msg[100] =
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A";
    cksum = gps->calcChecksum(msg, strlen(msg));
    gps->appendChecksum(msg, strlen(msg), sizeof(msg));
    BOOST_TEST(string(msg) == string(goodmsg));
    BOOST_TEST(gps->checksumOK(msg, strlen(msg)));

    // Given an array of RMC messages with various errors, make sure none
    // of them pass even though the checksum is correct.
    char badmsgs[][128] = {
        "digit missing from time",
        "$GPRMC,22009.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A",
        "digit missing from date",
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,11219,,,A",
        "digit replaced with random character",
        "$GPRMC,22000a.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A",
        "time digit replaced with negative sign",
        "$GPRMC,-20009.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A",
        "year digit replaced with negative sign",
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,1212-9,,,A",
        "month digit replaced with negative sign",
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,12-219,,,A",
        "month out of range",
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,121319,,,A",
        "day out of range",
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,001219,,,A",
        "decimal replaced with number",
        "$GPRMC,220009100,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A",
        "missing decimal",
        "$GPRMC,22000900,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A",
        "good message",
        "$GPRMC,220009.00,A,4002.29363,N,10514.51724,W,0.750,,121219,,,A"
    };

    dsm_time_t texp =
        UTime::parse(true, "2019 12 12 22:00:09.0000", "%Y %m %d %H:%M:%S.%4f")
            .toUsecs();
    unsigned long i = 0;
    while (i < sizeof(badmsgs) / sizeof(badmsgs[0]))
    {
        char* desc = badmsgs[i];
        std::cerr << desc << std::endl;
        char* imsg = badmsgs[++i];
        gps->appendChecksum(msg, strlen(imsg), sizeof(badmsgs[i]));
        dsm_time_t tt;
        tt = gps->parseRMC(imsg + 7, &(d[0]), d.size(), 1);
        // Returning the raw time means it did not parse.
        if (strcmp(desc, "good message"))
            BOOST_TEST(tt == 1);
        else
            BOOST_TEST(tt == texp);
        ++i;
    }
    delete gps;
#endif
}
