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
#include <cstring>
#include <sys/time.h>

#include <nidas/util/SerialPort.h>
#include <nidas/util/UTime.h>

using namespace std;

namespace n_u = nidas::util;

class GPS_SetClock {
public:
    GPS_SetClock();
    int parseRunstring(int argc, char** argv);
    int run();
    static int usage(const char* argv0);
    static const char* skipcommas(const char* cp, int ncomma);
    static void setSysTime(const n_u::UTime& tgps) throw(n_u::IOException);
private:
    string device;
    static int dataTimeoutDefault;
    int dataTimeout;
    static int lockTimeoutDefault;
    int lockTimeout;
    int baudRate;

};
int GPS_SetClock::dataTimeoutDefault = 30;
int GPS_SetClock::lockTimeoutDefault = 600;

GPS_SetClock::GPS_SetClock():
	dataTimeout(dataTimeoutDefault),
	lockTimeout(lockTimeoutDefault),
	baudRate(4800)
{
}

int GPS_SetClock::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "b:d:l:")) != -1) {
	switch (opt_char) {
	case 'b':
	    baudRate = atoi(optarg);
	    break;
	case 'l':
	    lockTimeout = atoi(optarg);
	    break;
	case 'd':
	    dataTimeout = atoi(optarg);
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);
    if (dataTimeout <= 0 || lockTimeout <= 0) return usage(argv[0]);
    return 0;
}

int GPS_SetClock::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-b baud] [-d data_timeout] [-l lock_timeout] device\n\
  -b baud: baud rate, default=4800\n\
  -d data_timeout: seconds to wait for $GPRMC data from device (default=" <<
  	dataTimeoutDefault << ")\n\
  -l lock_timeout: seconds to wait until receipt of a valid \'A\' $GPRMC recorddefault=" << lockTimeoutDefault << ")\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/gps0\n\
" << endl;
    return 1;
}

const char* GPS_SetClock::skipcommas(const char* cp, int ncomma)
{
    for (int i = 0; i < ncomma; i++) {
        if ((cp = strchr(cp,','))) cp++;
	else return 0;
    }
    return cp;
}

void GPS_SetClock::setSysTime(const n_u::UTime& tgps) throw(n_u::IOException)
{
    n_u::UTime tsys;
    struct timeval tv;
    tv.tv_sec = tgps.toUsecs() / n_u::UTime::USECS_PER_SEC;
    tv.tv_usec = tgps.toUsecs() % n_u::UTime::USECS_PER_SEC;
    if (settimeofday(&tv,0) < 0) 
	throw n_u::IOException("settimeofday",tgps.format(true),errno);
    cerr << "Sys time: " <<
	tsys.format(false,"%Y %b %d %H:%M:%S %Z") << endl <<
	"Gps time: " <<
	tgps.format(false,"%Y %b %d %H:%M:%S %Z") << endl <<
	"Gps-Sys:  " <<
	(tgps - tsys) / n_u::UTime::MSECS_PER_SEC << " millisec" << endl;
}

int GPS_SetClock::run()
{

    char status = '?';
    try {

	const int gpsLockReport = dataTimeout;

	cerr << "Reading " << device << " for $GPRMC 'A' record." << endl;
	cerr << "Timeouts: data=" << dataTimeout <<
		", lock ($GPRMC 'A')=" << lockTimeout <<
		" secs. Baud=" << baudRate << " bps" << endl;

	n_u::SerialPort gps(device);

	gps.setBaudRate(baudRate);
	gps.iflag() = ICRNL;
	gps.oflag() = OPOST;
	gps.lflag() = ICANON;

	gps.open(O_RDONLY);

	char inbuf[1024];

	n_u::UTime tgps;
	n_u::UTime tbegin;
	n_u::UTime deadline = n_u::UTime() +
		lockTimeout * n_u::UTime::USECS_PER_SEC;
	n_u::UTime reportTime = n_u::UTime() +
		gpsLockReport * n_u::UTime::USECS_PER_SEC;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(gps.getFd(),&readfds);
	int maxfd = gps.getFd() + 1;
	struct timeval timeout = { 10,0};

	int noDataTime = 0;

	n_u::UTime tnow;

	while ((tnow = n_u::UTime()) < deadline) {

	    if (tnow > reportTime) {
		int tdiff = (tnow - tbegin + n_u::UTime::USECS_PER_SEC/2) /
			n_u::UTime::USECS_PER_SEC;
		if (status == 'V') 
		    cerr << "$GPRMC status='V' for " << tdiff <<
		    	" seconds" << endl;
		else if (status == 'B') 
		    cerr << "No $GPRMC data for " << tdiff <<
		    	" seconds. Wrong baud rate?" << endl;
		else cerr << "No data for " << tdiff <<
			" seconds. Is GPS connected?" << endl;
		reportTime += gpsLockReport * n_u::UTime::USECS_PER_SEC;
	    }

	    int nfd;
	    struct timeval selto = timeout;
	    if ((nfd = ::select(maxfd,&readfds,0,0,&selto)) < 0)
	    	throw n_u::IOException(gps.getName(),"select",errno);

	    if (nfd == 0) {
		status = '?';
	        noDataTime += timeout.tv_sec;
		if (noDataTime >= dataTimeout) break;
		continue;
	    }

	    noDataTime = 0;

	    status = 'B';
	    int l = gps.readLine(inbuf,sizeof(inbuf));
	    // cerr << "l=" << l << endl << inbuf << endl;
	    if (l > 7 && !strncmp("$GPRMC,",inbuf,7)) {
		int hour,min,sec,day,mon,year;
	        const char* cp = inbuf + 7;
		if (sscanf(cp,"%2d%2d%2d",&hour,&min,&sec) != 3) continue;
		if (!(cp = skipcommas(cp,1))) continue;
		status = *cp;
		if (!(cp = skipcommas(cp,7)) ||
		    sscanf(cp,"%2d%2d%2d",&day,&mon,&year) != 3) {
		    status = 'B';
		    continue;
		}

		// cerr << status << ' ' << year << ' ' << mon << ' ' << day <<
		// 	' ' << hour << ':' << min << ':' << sec << endl;
		tgps = n_u::UTime(true,year+2000,mon,day,hour,min,sec,0);

		if (status == 'A') {
		    cerr << "$GPRMC status 'A' received" << endl;
		    setSysTime(tgps);
		    break;
		}
	    }
	}
	int tdiff = (n_u::UTime() - tbegin + n_u::UTime::USECS_PER_SEC/2) /
		n_u::UTime::USECS_PER_SEC;
	if (status == 'V') {
	    cerr << "$GPRMC status='V' for " << tdiff <<
		" seconds. Setting system clock anyway" << endl;
	    setSysTime(tgps);
	}
	else if (status == 'B')
	    cerr << "No $GPRMC records found for " << tdiff <<
	    	" seconds" << endl;
	else if (status == '?')
	    cerr << "No data received for " << tdiff << 
		    " seconds" << endl;
	gps.close();

    }
    catch(n_u::IOException& ioe) {
	cerr << ioe.what() << endl;
	return 2;
    }

    switch (status) {
    case 'A': return 0; 	// A-OK
    				// 1=usage
    case 'V': return 2; 	// no gps lock, but set time anyway
    case 'B': return 3; 	// bad GPS data
    default:
    case '?': return 4; 	// no GPS data
    }
}

int main(int argc, char** argv)
{

    GPS_SetClock setr;
    int stat;
    if ((stat = setr.parseRunstring(argc,argv)) != 0) return stat;
    return setr.run();
}
  
