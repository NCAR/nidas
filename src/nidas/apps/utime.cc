/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Date and time manipulation program, useful from shell scripts
    that need to parse time strings, do math and format output
    time strings.

 ********************************************************************
*/

#include <nidas/util/UTime.h>
#include <iostream>

using namespace std;
// using namespace nidas::util;
namespace n_u = nidas::util;

int usage(const char *argv0)
{
  cerr << "\
usage: " << argv0 << " [-l] [-L] date_time +[\"out_format\"]\n\
       " << argv0 << " [-l] [-L] date_time +\"in_format\" +[\"out_format\"]\n\
\n\
-l: local input. Interpret input time in local time zone, else GMT.\n\
-L: local output.  Print output time in local time zone, else GMT.\n\
\n\
date_time is one or more fields separated by spaces containing\n\
the date and time.\n\
\n\
in_format and out_format are optional time formats used by\n\
the nidas::util::UTime::format() method, containing ordinary characters,\n\
and any of the following %x format elements:\n\
    %Y   4 digit year\n\
    %y   2 digit year\n\
    %m   2 digit month (1-12)\n\
    %d   2 digit day of month (1-31)\n\
    %b   3 character month abbreviation, Jan, Feb, etc\n\
    %H %M %S   hour minute second\n\
    %nf   fractional seconds (nidas::util::UTime extension). n is digit\n\
        between 1 and 6, default is 3.\n\
    %Z   time zone (out_format only)\n\
    Do \"man strftime\" for more information.\n\
If in_format is specified (2nd usage), it describes the format of date_time.\n\
\n\
If in_format is not specified (1st usage), date_time contains date\n\
and time fields in order, starting with year, like one of the following\n\
    87 feb 1 11:22:33.9\n\
    87 feb 1 11:22\n\
    87 feb 1 112233\n\
    87 2 1 11:22:33\n\
    87 2 1 11:22\n\
    87 2 1 112233.9\n\
    87 2 1\n\
    539136000.9 (seconds since 1970 Jan 1 00:00 GMT)\n\
	now		(to get current time)\n\
\n\
If out_format is not specified, the output will be the number\n\
of seconds since 1970 Jan 01 00:00 GMT\n\
Examples:\n" <<
argv0 << " 1970 jan 1               prints 0\n" <<
argv0 << " -l 1970 jan 1            prints difference from GMT in seconds\n" <<
argv0 << " now +%y%m%d.dat          create a file name YYMMDD.dat using current time\n\
@ t = `utime now` - 86400; set f = `utime $t +%y%m%d.dat`\n\
  				csh shell commands to create yesterday\'s file\n\
				file name (86400 seconds in a day)." << endl;
    return 1;
}
int main(int argc, char **argv)
{
    const char *informat = 0;
    const char *outformat = 0;
    bool inUTC = true;
    bool outUTC = true;
    string dateTime;

    if (argc < 2) return usage(argv[0]);

    n_u::UTime ut;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"-h")) return usage(argv[0]);
        else if (!strcmp(argv[i],"-l")) inUTC = false;
        else if (!strcmp(argv[i],"-L")) outUTC = false;
        else if (argv[i][0] != '+') {
            if (dateTime.length() > 0) dateTime += ' ';
            dateTime += argv[i];
        }
        else if (!outformat) outformat = argv[i] + 1;
        else {
          informat = outformat;
          outformat = argv[i] + 1;
        }
    }

    if (!informat) ut = n_u::UTime::parse(inUTC,dateTime);
    else ut = n_u::UTime::parse(inUTC,dateTime,informat);

#ifdef UTIME_BASIC_STREAM_IO
    if (outUTC) cout << n_u::setTZ<char>("GMT");
#else
    if (outUTC) cout << n_u::setTZ("GMT");
#endif

    if (outformat && strlen(outformat) > 0) {
#ifdef UTIME_BASIC_STREAM_IO
        cout << n_u::setDefaultFormat<char>(outformat);
#else
        cout << n_u::setDefaultFormat(outformat);
#endif
        cout << ut << endl;
    }
    else cout <<  ut.toUsecs() / USECS_PER_SEC << endl;
}
