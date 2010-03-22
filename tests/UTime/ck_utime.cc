/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

 Test program for nidas::util::UTime time handling class.
*/

#include <nidas/util/UTime.h>

#include <iostream>

#include <cstdio>
#include <assert.h>
#include <time.h>

using namespace nidas::util;
using namespace std;

int main(int argc, char** argv)
{
    time_t now = ::time(0);       // current time
    struct tm tm;
    char timestr[64];
    string utstr;

    UTime ut(now);

    const char*zones[]={"","GMT","MST7MDT",""};

    for (unsigned int i = 0; i < sizeof(zones)/sizeof(zones[0]); i++) {

        UTime::setTZ(zones[i]);
        cout << "TZ=" << UTime::getTZ() << endl;

        string format("%Y %m %d %H:%M:%S");

        cout << "Checking UTime::format, UTC,\"" << format << "\" of time=now against gmtime_r, strftime ... ";

        utstr = ut.format(true,format);

        gmtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);

        if (utstr != timestr) {
            cerr << "\nut.format=\"" << utstr << "\"" << endl;
            cerr << "strftime =\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        format = "%Y %m %d %H:%M:%S %s";
        cout << "Checking UTime::format, localtime,\"" << format << "\" of time=now against localtime_r, strftime ... ";
        
        utstr = ut.format(false,format);

        localtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);

        if (utstr != timestr) {
            cerr << "\nut.format=\"" << utstr << "\"" << endl;
            cerr << "strftime =\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        cout << "Comparing UTime::format, UTC,\"%s\", now against time_t value ... ";
        utstr = ut.format(true,"%s");
        sprintf(timestr,"%ld",(long)now);

        if (utstr != timestr) {
            cerr << "\nutstr(%s)   =\"" << utstr << "\"" << endl;
            cerr << "timestr(%ld)=\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        cout << "Comparing UTime::format, UTC,\"%s\", now against toUsecs()/USECS_PER_SEC ... ";

        sprintf(timestr,"%lld",ut.toUsecs() / USECS_PER_SEC);
        if (utstr != timestr) {
            cerr << "\nutstr(%s)   =\"" << utstr << "\"" << endl;
            cerr << "timestr(%lld)=\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        cout << "Doing UTime::parse with %s, utc=false ...";
        UTime ut2 = UTime::parse(false,utstr,"%s");
        if (ut2 != ut) {
            ut2.setFormat(format);
            ut.setFormat(format);
            cerr << "ut2=" << ut2 << endl;
            cerr << "ut=" << ut << endl;
            return 1;
        }

        cout << "OK" << endl;
    }

    cout << "Checking daylight savings time switch ...";
    UTime::setTZ("MST7MDT");
    try {
        ut.set(false,"2010 mar 14 00:00","%Y %b %d %H:%M");
    }
    catch(const ParseException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    ut.setFormat("%H:%M:%S");
    ut.setUTC(false);

    // print out 01:00 and 02:00, which because of the daylight saving time switch is 03:00
    ut += USECS_PER_HOUR;
    utstr = ut.format();
    if (utstr != "01:00:00") {
        cerr << "ut=" << ut << ", should be 01:00:00 MST" << endl;
        return 1;
    }
    // UTC
    utstr = ut.format(true);
    if (utstr != "08:00:00") {
        cerr << "ut=" << ut << ", should be 08:00:00 UTC" << endl;
        return 1;
    }
    ut += USECS_PER_HOUR;
    utstr = ut.format();
    if (utstr != "03:00:00") {
        cerr << "ut=" << ut << ", should be 03:00:00 MDT" << endl;
        return 1;
    }
    // UTC
    utstr = ut.format(true);
    if (utstr != "09:00:00") {
        cerr << "ut=" << ut << ", should be 09:00:00 UTC" << endl;
        return 1;
    }
    cout << "OK" << endl;

    // check an absolute time
    utstr = "2004 07 15 17:35:15.003";
    string fmt = "%Y %m %d %H:%M:%S.%3f";
    cout << "Checking absolute time " << utstr << " UTC ... ";
    try {
        ut = UTime::parse(true,utstr,fmt);
    }
    catch(const ParseException& e) {
        cerr << e.what() << endl;
        return 1;
    }

    // hand calculation of above time from 1970 Jan 01 00:00 UTC
    // 2002 =  8*(4*365+1) days after 1970 Jan 1
    long long usecs = (long long)((8*(4*365+1) + 2*365 + (31+29+31+30+31+30+14)) * 86400 +
        (17*3600) + 35*60 + 15) * USECS_PER_SEC + 3 * USECS_PER_MSEC;

    assert(ut.toUsecs() == usecs);
    cout << "OK" << endl;

    string utstr2 = ut.format(true,fmt);
    if (utstr != utstr2)
        cerr << "formatted time = " << utstr2 << " is not equal to original time" << endl;
    assert(utstr == utstr2);

    cout << "Checking absolute time " << utstr << " :America/Phoenix ... ";
    UTime::setTZ(":America/Phoenix");
    try {
        ut = UTime::parse(false,utstr,fmt);
    }
    catch(const ParseException& e) {
        cerr << e.what() << endl;
        return 1;
    }

    assert(ut.toUsecs() == usecs + (7*3600LL) * USECS_PER_SEC);
    cout << "OK" << endl;

    utstr2 = ut.format(false,fmt);
    if (utstr != utstr2)
        cerr << "formatted time = " << utstr2 << " is not equal to original time" << endl;
    assert(utstr == utstr2);

    cout << "Success: " << argv[0] << endl;
    return 0;
}
