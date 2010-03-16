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

        cout << "Checking UTime::format, now, UTC, against gmtime_r, strftime ... ";

        utstr = ut.format(true,format);

        gmtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);

        if (utstr != timestr) {
            cerr << "\nut.format=\"" << utstr << "\"" << endl;
            cerr << "strftime =\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        cout << "Checking UTime::format, now, localtime, against localtime_r/strftime ... ";
        
        utstr = ut.format(false,format);

        localtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);

        if (utstr != timestr) {
            cerr << "\nut.format=\"" << utstr << "\"" << endl;
            cerr << "strftime =\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        cout << "Checking UTime::format, now, localtime, with %s format against localtime_r/strftime ... ";

        format = "%Y %m %d %H:%M:%S %s";

        utstr = ut.format(false,format);

        localtime_r(&now,&tm);
        strftime(timestr,sizeof(timestr),format.c_str(),&tm);
        if (utstr != timestr) {
            cerr << "\nut.format=\"" << utstr << "\"" << endl;
            cerr << "strftime =\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;

        cout << "Comparing UTime::format, now, localtime with %s format against time_t value ... ";
        utstr = ut.format(true,"%s");
        sprintf(timestr,"%ld",(long)now);

        if (utstr != timestr) {
            cerr << "\nutstr(%s)   =\"" << utstr << "\"" << endl;
            cerr << "timestr(%ld)=\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }

        sprintf(timestr,"%lld",ut.toUsecs() / USECS_PER_SEC);
        if (utstr != timestr) {
            cerr << "\nutstr(%s)   =\"" << utstr << "\"" << endl;
            cerr << "timestr(%lld)=\"" << timestr << "\"" << endl;
            assert(utstr == timestr);
        }
        cout << "OK" << endl;
    }

    cout << "Checking daylight savings time switch ...";
    UTime::setTZ("MST7MDT");
    ut.set(false,"2010 mar 14 00:00","%Y %b %d %H:%M");
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

    cout << "Success: " << argv[0] << endl;
    return 0;
}
