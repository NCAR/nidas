/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*
 Test program for nidas::util::UTime time handling class.
*/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

using nidas::util::LogConfig;
using nidas::util::LogScheme;

#include <iostream>

#include <cstdio>
#include <time.h>
#include <vector>

using namespace nidas::util;
using namespace std;


bool init_unit_test()
{
    boost::unit_test::framework::master_test_suite().p_name.value = "UTime";
    return true;
}


int
main( int argc, char* argv[] )
{
    Logger::setScheme(LogScheme("ck_utime").addConfig("debug"));
    DLOG(("debug enabled."));
    return ::boost::unit_test::unit_test_main(init_unit_test,
                                              argc, argv );
}


BOOST_AUTO_TEST_CASE(test_utime_constants)
{
    UTime min(UTime::MIN);
    UTime max(UTime::MAX);
    UTime zero(UTime::ZERO);

    BOOST_TEST(min == UTime(LONG_LONG_MIN));
    BOOST_TEST(max == UTime(LONG_LONG_MAX));
    BOOST_TEST(zero == UTime(0ll));
    BOOST_TEST(zero.isZero());
    BOOST_TEST(zero.isSet());

    BOOST_TEST(min == UTime::MIN);
    BOOST_TEST(max == UTime::MAX);

    UTime one;
    BOOST_TEST(!one.isZero());
    BOOST_TEST(one != min);
    BOOST_TEST(one != max);
    BOOST_TEST(one.isSet());

    BOOST_TEST(!max.isZero());

    // really this should be an implementation detail, but some prior usage
    // may depend on it.
    BOOST_TEST(!min.isZero());

    BOOST_TEST(min.isMin());
    BOOST_TEST(max.isMax());
    BOOST_TEST(!min.isMax());
    BOOST_TEST(!max.isMin());
    BOOST_TEST(!min.isSet());
    BOOST_TEST(!max.isSet());

    BOOST_CHECK_EQUAL(UTime::MIN.format(), "MIN");
    BOOST_CHECK_EQUAL(UTime::MAX.format(), "MAX");
}


BOOST_AUTO_TEST_CASE(test_utime)
{
    time_t now = ::time(0);       // current time

    struct tm tm;
    char timestr[64];
    string utstr;

    UTime ut(now);

    const char*zones[]={"","GMT","MST7MDT",""};

    for (unsigned int i = 0; i < sizeof(zones)/sizeof(zones[0]); i++) {

        UTime::setTZ(zones[i]);
        BOOST_TEST_MESSAGE("TZ=" << UTime::getTZ());

        string format("%Y %m %d %H:%M:%S");

        BOOST_TEST_MESSAGE("Checking UTime::format, UTC,\"" << format
                           << "\" of time=now against gmtime_r, "
                           << "strftime ... ");

        utstr = ut.format(true,format);

        gmtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);

        BOOST_TEST(utstr == timestr);

        format = "%Y %m %d %H:%M:%S %s";
        BOOST_TEST_MESSAGE("Checking UTime::format, localtime,'"
                           << format << "' of time=now against "
                           << "localtime_r, strftime ... ");

        utstr = ut.format(false,format);

        localtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);

        BOOST_TEST(utstr == timestr);

        BOOST_TEST_MESSAGE("Comparing UTime::format, UTC,'%s', "
                           "of time=now against time_t value ... ");
        utstr = ut.format(true,"%s");
        sprintf(timestr,"%ld",(long)now);

        BOOST_TEST(utstr == timestr);

        BOOST_TEST_MESSAGE("Comparing UTime::format, UTC,'%s', of "
                           "time=now against toUsecs()/USECS_PER_SEC ... ");

        sprintf(timestr,"%lld",ut.toUsecs() / USECS_PER_SEC);
        BOOST_TEST(utstr == timestr);

        BOOST_TEST_MESSAGE("Doing UTime::parse with %s, utc=false ...");
        UTime ut2 = UTime::parse(false, utstr, "%s");
        BOOST_TEST(ut2 == ut);
    }

    BOOST_TEST_MESSAGE("Checking daylight savings time switch ... ");
    UTime::setTZ("MST7MDT");
    // should not throw
    ut.set(false,"2010 mar 14 00:00","%Y %b %d %H:%M");
    ut.setFormat("%H:%M:%S");
    ut.setUTC(false);

    // print out 01:00 and 02:00, which because of the daylight saving time switch is 03:00
    ut += USECS_PER_HOUR;
    utstr = ut.format();
    BOOST_TEST(utstr == "01:00:00");
    // UTC
    utstr = ut.format(true);
    BOOST_TEST(utstr == "08:00:00");
    ut += USECS_PER_HOUR;
    utstr = ut.format();
    BOOST_TEST(utstr == "03:00:00");
    // UTC
    utstr = ut.format(true);
    BOOST_TEST(utstr == "09:00:00");

    // check an absolute time
    utstr = "2004 07 15 17:35:15.003";
    string fmt = "%Y %m %d %H:%M:%S.%3f";
    BOOST_TEST_MESSAGE("Checking absolute time " << utstr << " UTC ... ");
    ut = UTime::parse(true,utstr,fmt);

    // hand calculation of above time from 1970 Jan 01 00:00 UTC
    // 2002 =  8*(4*365+1) days after 1970 Jan 1
    long long usecs = (long long)((8*(4*365+1) + 2*365 + (31+29+31+30+31+30+14)) * 86400 +
        (17*3600) + 35*60 + 15) * USECS_PER_SEC + 3 * USECS_PER_MSEC;

    BOOST_TEST(ut.toUsecs() == usecs);

    string utstr2 = ut.format(true,fmt);
    BOOST_TEST(utstr == utstr2);

    BOOST_TEST_MESSAGE("Checking absolute time " << utstr
                       << " :America/Phoenix ... ");
    UTime::setTZ(":America/Phoenix");
    ut = UTime::parse(false,utstr,fmt);

    BOOST_TEST(ut.toUsecs() == usecs + (7*3600LL) * USECS_PER_SEC);

    utstr2 = ut.format(false,fmt);
    BOOST_TEST(utstr == utstr2);

    // check times before and after Jan 1 1970
    utstr = "1970 01 01 00:00:00.000";
    fmt = "%Y %m %d %H:%M:%S.%3f";
    BOOST_TEST_MESSAGE("Checking time " << utstr << " UTC ... ");
    ut = UTime::parse(true,utstr,fmt);
    utstr2 = ut.format(true,fmt);
    BOOST_TEST(utstr == utstr2);

    BOOST_TEST_MESSAGE("Checking conversion to US/Eastern "
                       << utstr << " UTC ... ");
    UTime::setTZ("US/Eastern");
    utstr2 = ut.format(false,fmt);
    utstr = "1969 12 31 19:00:00.000";
    BOOST_TEST(utstr == utstr2);
}


BOOST_AUTO_TEST_CASE(test_near_epoch)
{
    UTime ut{ UTime::ZERO };
    string fmt = "%Y %m %d %H:%M:%S.%6f";

    BOOST_TEST_MESSAGE("Checking formatting and parsing of "
                       "times around 1970 Jan 1 UTC ... ");
    int ncheck = 0;

    vector<int> useconds { 0, 1, 9, 499, 499999, 500001, 999000, 999999 };
    for (long long sec = -86400 * 3 / 2; sec <= 86400 * 3 / 2; sec += 3599) {

        for (auto usec: useconds) {

            UTime utx = ut + sec * USECS_PER_SEC + usec;
            string utstr2 = utx.format(true,fmt);
            BOOST_TEST_MESSAGE("Secs=" << sec << ", usec=" << usec << ": "
                               << utx << " as '" << fmt << "':" << utstr2);
            UTime utx2 = UTime::parse(true,utstr2,fmt);
            utx.setFormat("%Y-%m-%d,%H:%M:%S.%6f");
            utx2.setFormat("%Y-%m-%d,%H:%M:%S.%6f");
            BOOST_TEST(utx == utx2, "checking " << utx << "==" << utx2);

            ncheck++;
        }
    }
}


BOOST_AUTO_TEST_CASE(test_parse_trailing_separator)
{
    // make sure a trailing separator is not parsed
    int nparsed{0};
    UTime when = UTime::parse("2019-11-07T", &nparsed);
    BOOST_CHECK_EQUAL(nparsed, 10);
    BOOST_CHECK_EQUAL(when, UTime(true, 2019, 11, 7, 0, 0, 0));

    when = UTime::parse("2019-11-07 ", &nparsed);
    BOOST_CHECK_EQUAL(nparsed, 10);
    BOOST_CHECK_EQUAL(when, UTime(true, 2019, 11, 7, 0, 0, 0));

    // a Z suffix should not be parsed if utc is false, but it won't throw an
    // exception.
    when = UTime::parse(false, "2019-11-08Z", &nparsed);
    BOOST_CHECK_EQUAL(nparsed, 10);
    BOOST_CHECK_EQUAL(when, UTime(false, 2019, 11, 8, 0, 0, 0));
}


void test_precisions(const UTime& ut, const string& expected)
{
    for (int i = 0; i < 10; ++i) {
        string fmt = "%Y-%m-%d,%H:%M:%S.%" + to_string(i) + "f";
        // make sure precision is capped at 6.  if n is zero, then expect no
        // digits for fractional seconds.
        size_t len = expected.size() - std::max((6 - i), 0);
        string subx = expected.substr(0, len);
        BOOST_TEST(ut.format(true, fmt) == subx, 
                   expected << " with precision " << i << ": " << subx);
    }
}



BOOST_AUTO_TEST_CASE(test_format_fractions)
{
    UTime ut(true, 2025, 3, 5, 8, 18, 59, 456789);
    test_precisions(ut, "2025-03-05,08:18:59.456789");

    ut = UTime(true, 2025, 3, 5, 8, 18, 59, 999999);
    test_precisions(ut, "2025-03-05,08:18:59.999999");

    ut = UTime(true, 2025, 3, 5, 8, 18, 59, 555500);
    test_precisions(ut, "2025-03-05,08:18:59.555500");

    ut = UTime(true, 2025, 3, 5, 8, 18, 59, 500);
    test_precisions(ut, "2025-03-05,08:18:59.000500");

    ut = UTime(true, 2025, 3, 5, 8, 18, 59, 499999);
    test_precisions(ut, "2025-03-05,08:18:59.499999");

    ut = UTime(true, 2025, 3, 5, 8, 18, 59, 500000);
    test_precisions(ut, "2025-03-05,08:18:59.500000");

    ut = UTime(true, 1969, 12, 31, 8, 18, 59, 500000);
    test_precisions(ut, "1969-12-31,08:18:59.500000");
}


BOOST_AUTO_TEST_CASE(test_negatives)
{
    UTime epoch = UTime::parse(true, "1970 01 01 00:00:00.000");
    BOOST_TEST(epoch.isZero());

    string fmt{ "%Y-%m-%d %H:%M:%S" };
    long long seconds = -86400;
    UTime ut = epoch + seconds * USECS_PER_SEC;
    BOOST_TEST(ut.format(true, fmt) == "1969-12-31 00:00:00");

    ut = epoch + -1 * USECS_PER_SEC + 999999;
    BOOST_TEST(ut.format(true, fmt) == "1969-12-31 23:59:59");
    BOOST_TEST(ut.format(true, fmt+".%6f") == "1969-12-31 23:59:59.999999");
}


BOOST_AUTO_TEST_CASE(test_default_parsing_formats)
{
    typedef std::pair<std::string, UTime> format_pair_t;
    typedef std::vector<format_pair_t> cases_t;
    cases_t cases;
    cases.push_back(make_pair("2019-11-07T16:10:55.001",
                              UTime(true, 2019, 11, 7, 16, 10, 55.001)));
    cases.push_back(make_pair("2019-11-07_16:10:55.001Z",
                              UTime(true, 2019, 11, 7, 16, 10, 55.001)));
    cases.push_back(make_pair("2019-11-07 16:10:55.001",
                              UTime(true, 2019, 11, 7, 16, 10, 55.001)));
    cases.push_back(make_pair("2019-11-07 16:10:55.124050",
                              UTime(true, 2019, 11, 7, 16, 10, 55.12405)));
    cases.push_back(make_pair("2019-11-07 16:10:55.124050Z",
                              UTime(true, 2019, 11, 7, 16, 10, 55.12405)));
    // Make sure shortened forms handled correctly too.
    cases.push_back(make_pair("2019-11-07 16:10:55",
                              UTime(true, 2019, 11, 7, 16, 10, 55)));
    cases.push_back(make_pair("2019-11-07 16:10:55Z",
                              UTime(true, 2019, 11, 7, 16, 10, 55)));
    cases.push_back(make_pair("2019-11-07 16:10",
                              UTime(true, 2019, 11, 7, 16, 10, 0)));
    cases.push_back(make_pair("2019-11-07_16:10",
                              UTime(true, 2019, 11, 7, 16, 10, 0)));
    cases.push_back(make_pair("2019-11-07",
                              UTime(true, 2019, 11, 7, 0, 0, 0)));
    cases.push_back(make_pair("2019-11-07Z",
                              UTime(true, 2019, 11, 7, 0, 0, 0)));

    string fmt{ "%Y-%m-%d %H:%M:%S.%6f" };
    for (cases_t::iterator cit = cases.begin(); cit != cases.end(); ++cit)
    {
        // use convert() because the whole string should be parsed
        BOOST_TEST_CHECKPOINT("Checking " << cit->first << " ... ");
        UTime ut = UTime::convert(cit->first);
        BOOST_TEST(ut == cit->second,
                   "parsing " << cit->first << " expects "
                   << cit->second.format(true, fmt)
                   << ": got " << ut.format(true, fmt));
    }
}


BOOST_AUTO_TEST_CASE(test_rounding)
{
    UTime ut = UTime::parse(true, "2019-11-07T16:10:55.001");
    UTime rounded{UTime::ZERO};
    rounded = ut.earlier(USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:10:55"));
    rounded = ut.earlier(5*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:10:55"));
    rounded = ut.earlier(10*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:10:50"));
    rounded = ut.round(5*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:10:55"));
    rounded = ut.round(10*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:11:00"));

    ut = UTime::parse(true, "2019-11-07T16:10:59.567");
    rounded = ut.round(USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:11:00"));
    rounded = ut.round(5*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:11:00"));
    rounded = ut.round(10*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:11:00"));
    rounded = ut.round(5*60*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "2019-11-07T16:10:00"));

    // test before epoch also
    ut = UTime::parse(true, "1960-11-07T16:10:59.567");
    rounded = ut.round(USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "1960-11-07T16:11:00"));
    rounded = ut.round(5*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "1960-11-07T16:11:00"));
    rounded = ut.round(10*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "1960-11-07T16:11:00"));
    rounded = ut.round(5*60*USECS_PER_SEC);
    BOOST_TEST(rounded == UTime::parse(true, "1960-11-07T16:10:00"));

    // check the trivial case
    BOOST_TEST(ut.round(0) == ut);
    BOOST_TEST(ut.earlier(0) == ut);
}
