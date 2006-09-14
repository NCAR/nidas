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
 * Program to set options on a Garmin GPS.
 * Written using docs for a Garmin model 35.
 */

#include <iostream>
#include <set>

#include <nidas/util/SerialPort.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/UTime.h>

using namespace std;

namespace n_u = nidas::util;

class Garmin {
public:
    Garmin();

    int parseRunstring(int argc, char** argv);

    int run();

    static int usage(const char* argv0);

    string getStringField(const string& str, int nfield);

    bool setBaudRateOption();

    void enablePPS();

    void disablePPS();

    bool enableMessage(const string& msg);

    bool disableMessage(const string& msg);

    bool disableAllMessages();

    bool enableAllMessages();

    bool checkMessage(const string& msg);

    void scanMessages(int seconds) throw(n_u::IOException);

    string readMessage() throw(n_u::IOException);

    static int getBaudRateIndex(int rate);

    static int getBaudRate(int index);

private:
    string device;

    int baudRate;

    int newBaudRate;

    bool ppsEnable;

    bool ppsDisable;

    int pulseWidth;

    bool disableAllMsg;

    n_u::SerialPort gps;

    fd_set readfds;

    int maxfd;

    /** Messages to be disabled */
    set<string> toDisable;

    /** Messages to be enabled */
    set<string> toEnable;

    set<string> currentMessages;

    static struct baudRates {
        int rate;
	int index;
    } baudRateTable[];

};

Garmin::baudRates Garmin::baudRateTable[] = {
    {  300, 6 },
    {  600, 7 },
    { 1200, 1 },
    { 2400, 2 },
    { 4800, 3 },
    { 9600, 4 },
    {19200, 5 }
};

/* static */
int Garmin::getBaudRateIndex(int rate)
{
    unsigned int nrate = sizeof(baudRateTable)/sizeof(baudRateTable[0]);;
    for (unsigned int i = 0; i < nrate; i++)
        if (baudRateTable[i].rate == rate) return baudRateTable[i].index;
    return -1;
}
       
/* static */
int Garmin::getBaudRate(int index)
{
    unsigned int nrate = sizeof(baudRateTable)/sizeof(baudRateTable[0]);;
    for (unsigned int i = 0; i < nrate; i++)
        if (baudRateTable[i].index == index) return baudRateTable[i].rate;
    return -1;
}
       
Garmin::Garmin():
	baudRate(4800),
	newBaudRate(-1),
	ppsEnable(false),
	ppsDisable(false),
	pulseWidth(-1),
	disableAllMsg(false)
{
}

int Garmin::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "b:B:d:De:np:")) != -1) {
	switch (opt_char) {
	case 'b':
	    baudRate = atoi(optarg);
	    break;
	case 'B':
	    newBaudRate = atoi(optarg);
	    {
		int baudRateIndex = getBaudRateIndex(newBaudRate);
		if (baudRateIndex < 0) {
		    cerr << "Unsupported baud rate: " << newBaudRate << endl;
		    return usage(argv[0]);
		}
	    }
	    break;
	case 'd':
	    if (!checkMessage(optarg)) return usage(argv[0]);
	    toDisable.insert(optarg);
	    break;
	case 'D':
	    disableAllMsg = true;
	    break;
	case 'e':
	    if (!checkMessage(optarg)) return usage(argv[0]);
	    toEnable.insert(optarg);
	    break;
	case 'n':
	    ppsDisable = true;
	    break;
	case 'p':
	    pulseWidth = atoi(optarg);
	    pulseWidth = (pulseWidth / 20) - 1;
	    if (pulseWidth < 0 || pulseWidth > 48)
	    	return usage(argv[0]);
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);
    return 0;
}

int Garmin::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-b baud] [-B baud] [-d msg ...] [-e msg ...] [-n | -p pw] device\n\
  -b baud: baud rate, default=4800\n\
  -B baud: set Garmin baud rate (300,600,1200,2400,4800,9600,19200) \n\
  -d msg: disable output message (can specify more than one -d option)\n\
  -D: disable all output messages\n\
  -e msg: enable output message (can specify more than one -e option)\n\
  -n: disable PPS output\n\
  -p pw: enable PPS output, pw=pulse width in msec (20 to 980)\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/gps0\n\
\n\
\n\
  Message types that can be enabled/disabled:\n\
GPGGA: GPS Fix Data\n\
	time,lat,lon,qual,nsat,hordil,alt,geoidht,diffage,diffid\n\
GPGSA: DOP and Active Satellites\n\
 	mode,fix,sat,sat,sat,...,posdil,hordil,verdil\n\
GPGSV: Satellites in View\n\
GPRMC: Recommended Minimum (needed by NTP)\n\
 	time,status,lat,lon,sog,cog,date,magvar,magdir,mode\n\
GPVTG: Track Made Good and Ground Speed\n\
	cog,magcog,knots,km/hr,mode\n\
PGRMB: DiffGPS Beacon Information (Garmin proprietary)\n\
PGRME: Estimated Error Information (Garmin proprietary)\n\
PGRMF: GPS Fix Data Sentence (Garmin proprietary)\n\
PGRMT: Sensor Status Information (Garmin proprietary)\n\
PGRMV: 3D Velocity Information (Garmin proprietary)\n\
LCGLL: Geographic Position with LORAN Talker ID\n\
LCVTG: Track Made Good and Ground Speed with LORAN Talker ID\n\
" << endl;
    return 1;
}

string Garmin::readMessage() throw(n_u::IOException)
{
    int nfd;
    struct timeval timeout = { 2,0};

    fd_set fds = readfds;
    if ((nfd = ::select(maxfd,&fds,0,0,&timeout)) < 0)
	throw n_u::IOException(gps.getName(),"select",errno);

    if (nfd == 0)
	throw n_u::IOTimeoutException(gps.getName(),"read");
    char buf[256];
    int l = gps.readLine(buf,sizeof(buf));
    return string(buf,buf+l);
}
  
bool Garmin::checkMessage(const string& msg)
{
    if (msg == "GPGGA" || msg == "GPGSA" || msg == "GPGSV" ||
	msg == "GPRMC" || msg == "GPVTG" || msg == "PGRMB" ||
	msg == "PGRME" || msg == "PGRMF" || msg == "PGRMT" ||
	msg == "PGRMV" || msg == "LCGLL" || msg == "LCVTG") return true;
    return false;
}

string Garmin::getStringField(const string& str, int nfield)
{
    string::size_type nc1 = 0;
    for (int i = 0; i < nfield; i++) {
	nc1 = str.find(',',nc1);
	if (nc1 == string::npos) return "";
    }
    string::size_type nc2 = str.find(',',nc1);
    return str.substr(nc1,nc2);
}

bool Garmin::setBaudRateOption() 
{
    int baudRateIndex = getBaudRateIndex(newBaudRate);
    if (baudRateIndex <= 0) {
	cerr << "Baud baud rate: " << newBaudRate << endl;
	return false;
    }

    ostringstream ost;
    ost << "$PGRMC,,,,,,,,,," << baudRateIndex << ",,,,\r\n";
    string str = ost.str();
    gps.write(str.c_str(),str.length());

    // read answer back
    for (int i = 0; i < 2; i++) {
	try {
	    str = readMessage();
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "setBaudRateOption: " << e.what() << endl;
	    return false;
	}
	if (str.substr(0,6) == "$PGRMC") {
	    str = getStringField(str,10);
	    if (str.length() > 0) {
		istringstream ist(str);
		int bi;
		ist >> bi;
		if (bi == baudRateIndex) return true;
		cerr << "Read back wrong baud rate index=" << bi <<
		    " rate=" << getBaudRate(bi) << endl;
		return false;
	    }
	}
    }

    cerr << "setBaudRateOption: no $PGRMC message received" << endl;
    return false;
}

void Garmin::enablePPS()
{
    ostringstream ost;
    ost << "$PGRMC,,,,,,,,,,,,2," << pulseWidth << ",\r\n";
    string str = ost.str();
    gps.write(str.c_str(),str.length());

    for (int i = 0; i < 10; i++) {
	try {
	    str = readMessage();
	    cerr << "enablePPS: " << str << endl;
	    if (str.substr(0,6) == "$PGRMC") break;
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "enablePPS: " << e.what() << endl;
	    break;
	}
    }
    ost.str("");
    ost << "$PGRMCE\r\n";
    str = ost.str();
    gps.write(str.c_str(),str.length());

    for (int i = 0; i < 10; i++) {
	try {
	    str = readMessage();
	    cerr << "enablePPS: " << str << endl;
	    if (str.substr(0,6) == "$PGRMC") break;
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "enablePPS: " << e.what() << endl;
	    break;
	}
    }
}

void Garmin::disablePPS()
{
    ostringstream ost;
    ost << "$PGRMC,,,,,,,,,,,,1,,\r\n";
    string str = ost.str();
    gps.write(str.c_str(),str.length());

    for (int i = 0; i < 10; i++) {
	try {
	    str = readMessage();
	    cerr << "disablePPS: " << str << endl;
	    if (str.substr(0,6) == "$PGRMC") break;
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << e.what() << endl;
	    cerr << "disablePPS: " << e.what() << endl;
	    break;
	}
    }
    ost.str("");
    ost << "$PGRMCE\r\n";
    str = ost.str();
    gps.write(str.c_str(),str.length());

    for (int i = 0; i < 10; i++) {
	try {
	    str = readMessage();
	    cerr << "disablePPS: " << str << endl;
	    if (str.substr(0,6) == "$PGRMC") break;
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << e.what() << endl;
	    cerr << "disablePPS: " << e.what() << endl;
	    break;
	}
    }

}

bool Garmin::enableMessage(const string& msg)
{
    ostringstream ost;
    ost << "$PGRMO," << msg << ",1\r\n";
    string str = ost.str();
    gps.write(str.c_str(),str.length());

    for (int i = 0; i < 5; i++) {
	try {
	    str = readMessage();
	    cerr << "enable " << msg << ": " << str << endl;
	    if (str.substr(1,5) == msg) return true;
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "enable " << msg << ": " << e.what() << endl;
	    break;
	}
    }
    cerr << "message=" << msg << " not read back after being enabled" << endl;
    return false;
}

bool Garmin::disableAllMessages()
{
    ostringstream ost;
    ost << "$PGRMO,,2\r\n";
    string str = ost.str();
    gps.write(str.c_str(),str.length());
    for (int i = 0; i < 2; i++) {
	try {
	    str = readMessage();
	    cerr << "disableAllMessages " << str << endl;
	}
	catch(const n_u::IOTimeoutException& e) {
	    return true;	// expected timeout
	}
    }
    cerr << "disableAllMessages not successful" << endl;
    return false;
}

bool Garmin::enableAllMessages()
{
    ostringstream ost;
    ost << "$PGRMO,,3\r\n";
    string str = ost.str();
    gps.write(str.c_str(),str.length());
    for (int i = 0; i < 2; i++) {
	try {
	    str = readMessage();
	    cerr << "enableAllMessages " << str << endl;
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "enableAllMessages: " << e.what() << endl;
	    if (i > 0) return false;
	}
    }
    return true;
}

bool Garmin::disableMessage(const string& msg)
{
    ostringstream ost;
    ost << "$PGRMO," << msg << ",0\r\n";
    string str = ost.str();

    gps.write(str.c_str(),str.length());

    n_u::UTime tmsg;
    for (int i = 0; i < 5 && (n_u::UTime() - tmsg) < 3 * n_u::UTime::USECS_PER_SEC; i++) {
	try {
	    str = readMessage();
	    cerr << "disable " << msg << ": " << str << endl;
	    if (str.substr(1,5) == msg) tmsg = n_u::UTime();
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "enableMessage: " << e.what() << endl;
	    return true;
	}
    }
    if ((n_u::UTime() - tmsg) < 3 * n_u::UTime::USECS_PER_SEC) {
	cerr << "message=" << msg << " not disabled successfully" << endl;
	return false;
    }
    return true;
}

void Garmin::scanMessages(int seconds) throw(n_u::IOException)
{
    n_u::UTime tthen = n_u::UTime() + seconds * n_u::UTime::USECS_PER_SEC;

    currentMessages.clear();

    while (n_u::UTime() < tthen) {
	try {
	    string str = readMessage();
	    if (str[0] == '$' && str.length() > 5)
		currentMessages.insert(str.substr(1,5));
	}
	catch(const n_u::IOTimeoutException& e) {
	    cerr << "scanMessages: " << e.what() << endl;
	}
    }

}
int Garmin::run()
{
    try {

	gps.setName(device);

	gps.setBaudRate(baudRate);
	gps.iflag() = ICRNL;
	gps.oflag() = OPOST;
	gps.lflag() = ICANON;

	gps.open(O_RDWR);

	FD_ZERO(&readfds);
	FD_SET(gps.getFd(),&readfds);

	maxfd = gps.getFd() + 1;

	// read messages for 5 seconds
	scanMessages(5);

	if (newBaudRate >= 0) setBaudRateOption();

	if (ppsDisable) disablePPS();
	else if (pulseWidth >= 0) enablePPS();

	set<string>::const_iterator mi = toDisable.begin();
	for ( ; mi != toDisable.end(); ++mi) 
	    if (!disableMessage(*mi)) return 1;

	for ( ; mi != toEnable.end(); ++mi) 
	    if (!enableMessage(*mi)) return 1;

	gps.close();

    }
    catch(n_u::IOException& ioe) {
	cerr << ioe.what() << endl;
	return 2;
    }
    return 0;
}

int main(int argc, char** argv)
{

    Garmin garmin;
    int stat;
    if ((stat = garmin.parseRunstring(argc,argv)) != 0) return stat;

    return garmin.run();
}

