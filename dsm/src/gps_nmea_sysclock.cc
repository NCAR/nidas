/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-03 16:55:51 -0700 (Fri, 03 Feb 2006) $

    $LastChangedRevision: 3276 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/src/sensor_sim.cc $
 ********************************************************************
*/

/*
 * This is a simple program to read NMEA messages from a
 * GPS connected to a serial port and set the system clock.
 * This is a crude way to do a big step change to the system clock,
 * and isn't meant to be accurate to better than a second
 * or two.
 * The GPS NMEA reference clock driver (127.127.20.0) in NTP
 * will only do relatively small step changes to the system clock.
 * If the system clock differs by more than about a
 * half hour from the GPS, then the NTP driver won't correct it.
 * So one can run this program to get the clock somewhat close
 * and then start the NTP driver.
 */

#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/time.h>

#include <atdTermio/SerialPort.h>
#include <atdUtil/UTime.h>


using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);
    static void usage(const char* argv0);
    string device;
};

Runstring::Runstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "")) != -1) {
	switch (opt_char) {
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) usage(argv[0]);
    if (optind != argc) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "device\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/gps0\n\
" << endl;
    exit(1);
}

const char* skipcommas(const char* cp, int ncomma)
{
    for (int i = 0; i < ncomma; i++) {
        if ((cp = strchr(cp,','))) cp++;
	else return 0;
    }
    return cp;
}

void setSysTime(const atdUtil::UTime& tgps) throw(atdUtil::IOException)
{
    atdUtil::UTime tsys;
    struct timeval tv;
    tv.tv_sec = tgps.toUsecs() / atdUtil::UTime::USECS_PER_SEC;
    tv.tv_usec = tgps.toUsecs() % atdUtil::UTime::USECS_PER_SEC;
    if (settimeofday(&tv,0) < 0) 
	throw atdUtil::IOException("settimeofday",tgps.format(true),errno);
    cerr << "Sys time: " <<
	tsys.format(false,"%Y %b %d %H:%M:%S %Z") << endl <<
	"Gps time: " <<
	tgps.format(false,"%Y %b %d %H:%M:%S %Z") << endl <<
	"Gps-Sys:  " <<
	(tgps - tsys) / atdUtil::UTime::MSECS_PER_SEC << " millisec" << endl;
}

int main(int argc, char** argv)
{
  
    Runstring rstr(argc,argv);
    char status = '?';

    try {

	const int MAXTIME = 30;

	cerr << "Reading " << rstr.device << " until valid data or " << MAXTIME << " elapsed secs" << endl;

	atdTermio::SerialPort gps(rstr.device);

	gps.setBaudRate(4800);
	gps.iflag() = ICRNL;
	gps.oflag() = OPOST;
	gps.lflag() = ICANON;

	gps.open(O_RDONLY);

	char inbuf[1024];

	atdUtil::UTime tgps;
	atdUtil::UTime tbeg;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(gps.fd(),&readfds);
	int maxfd = gps.fd() + 1;
	struct timeval timeout = { 10,0};
	int elapsedTime = 0;

	while (elapsedTime < MAXTIME) {
	    int nfd;
	    struct timeval selto = timeout;
	    if ((nfd = ::select(maxfd,&readfds,0,0,&selto)) < 0)
	    	throw atdUtil::IOException(gps.getName(),"select",errno);

	    elapsedTime = (atdUtil::UTime() - tbeg) /
	    	atdUtil::UTime::USECS_PER_SEC;

	    if (nfd == 0) continue;

	    int l = gps.readline(inbuf,sizeof(inbuf));
	    // cerr << "l=" << l << endl << inbuf << endl;
	    if (l > 7 && !strncmp("$GPRMC,",inbuf,7)) {
		int hour,min,sec,day,mon,year;
	        const char* cp = inbuf + 7;
		if (sscanf(cp,"%2d%2d%2d",&hour,&min,&sec) != 3) continue;
		if (!(cp = skipcommas(cp,1))) continue;
		status = *cp;
		if (!(cp = skipcommas(cp,7))) continue;
		if (sscanf(cp,"%2d%2d%2d",&day,&mon,&year) != 3) {
		    status = 'B';
		    continue;
		}

		// cerr << status << ' ' << year << ' ' << mon << ' ' << day <<
		// 	' ' << hour << ':' << min << ':' << sec << endl;
		tgps = atdUtil::UTime(true,year+2000,mon,day,hour,min,sec,0);

		if (status == 'A') {
		    setSysTime(tgps);
		    break;
		}
	    }
	}
	if (status == 'V') {
	    cerr << "Invalid $GPRMC status: " << status <<
	    	" after " << elapsedTime <<
		" seconds. Setting system clock anyway" << endl;
	    setSysTime(tgps);
	}
	else if (status != 'A')
	    cerr << "Timeout: " << elapsedTime <<
	    	" seconds reading GPS" << endl;
	gps.close();

    }
    catch(atdUtil::IOException& ioe) {
	cerr << ioe.what() << endl;
	return 2;
    }

    switch (status) {
    case 'A': return 0; 	// A-OK
    case 'V': return 1; 	// no gps lock, but set time anyway
    case 'B': return 2; 	// bad GPS data
    default:
    case '?': return 3; 	// no GPS data
    }
}

