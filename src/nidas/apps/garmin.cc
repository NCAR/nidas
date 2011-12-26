// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 Copyright 2005 UCAR, NCAR, All Rights Reserved

 $LastChangedDate$

 $LastChangedRevision$

 $LastChangedBy$

 $HeadURL$
 ********************************************************************
 */

/*
 * Program to set options on a Garmin GPS.
 * Written using docs for a Garmin model 35.
 */

#include <iostream>
#include <iomanip>
#include <set>

#include <nidas/util/SerialPort.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/UTime.h>
#include <nidas/util/util.h>

#include <cstdlib>  // atoi()

using namespace std;

namespace n_u = nidas::util;

class Garmin {
public:
    Garmin();

    int parseRunstring(int argc, char** argv);

    int run();

    static int usage(const char* argv0);

    string getStringField(const string& str, int nfield);

    bool setBaudRateOption() throw(n_u::IOException);

    int checkPPS(int* pulseWidth) throw(n_u::IOException);

    bool enablePPS() throw(n_u::IOException);

    bool disablePPS() throw(n_u::IOException);

    bool enableMessage(const string& msg) throw(n_u::IOException);

    bool disableMessage(const string& msg) throw(n_u::IOException);

    bool disableAllMessages() throw(n_u::IOException);

    bool enableAllMessages() throw(n_u::IOException);

    bool checkMessage(const string& msg);

    bool scanMessages(int seconds) throw(n_u::IOException);

    /**
     * Read PGRMI message, containing board init information
     * (position and time used from satellite acquisition).
     */
    bool readInit() throw(n_u::IOException);

    /**
     * Send PGRMI message, containing board init information
     * (position and time used from satellite acquisition).
     */
    bool sendInit(float lat, float lon) throw(n_u::IOException);

    string readMessage() throw(n_u::IOException);

    static int getBaudRateIndex(int rate);

    static int getBaudRate(int index);

    static string substCRNL(const string& str);

    static int getPulseWidth(int index);

    static int getPulseWidthIndex(int width);

private:
    string device;

    int baudRate;

    int newBaudRate;

    bool ppsEnable;

    bool ppsDisable;

    int pulseWidth;

    bool enableAllMsg;

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

    bool rescanMessages;

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

int Garmin::getPulseWidth(int index)
{
    return (index + 1) * 20;
}

int Garmin::getPulseWidthIndex(int width)
{
    return (width / 20) - 1;
}

Garmin::Garmin():
    device(),
    baudRate(4800),
    newBaudRate(-1),
    ppsEnable(false),
    ppsDisable(false),
    pulseWidth(-1),
    enableAllMsg(false),
    disableAllMsg(false),
    gps(),readfds(),maxfd(0),
    toDisable(),toEnable(),
    rescanMessages(true)
{
}

int Garmin::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "b:B:cd:De:Enp:")) != -1) {
        switch (opt_char) {
        case 'b':
            baudRate = atoi(optarg);
            break;
        case 'B':
            newBaudRate = atoi(optarg);
            {
                int baudRateIndex = getBaudRateIndex(newBaudRate);
                if (baudRateIndex < 0) {
                    cerr << "ERROR: Unsupported baud rate: " << newBaudRate << endl;
                    return usage(argv[0]);
                }
            }
            break;
        case 'd':
            if (!checkMessage(optarg)) {
                cerr << "ERROR: " <<  optarg << " is not a supported message" << endl;
                return usage(argv[0]);
            }
            toDisable.insert(optarg);
            break;
        case 'D':
            disableAllMsg = true;
            break;
        case 'e':
            if (!checkMessage(optarg)) {
                cerr << "ERROR: " <<  optarg << " is not a supported message" << endl;
                return usage(argv[0]);
            }
            toEnable.insert(optarg);
            break;
        case 'E':
            enableAllMsg = true;
            break;
        case 'n':
            ppsDisable = true;
            break;
        case 'p':
            pulseWidth = atoi(optarg);
            pulseWidth = getPulseWidthIndex(pulseWidth);
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
        Usage: " << argv0 << "[-b baud] [-B baud] [-d msg ...] [-D] [-e msg ...] [-E] [-n | -p pw] device\n\
        -b baud: baud rate, default=4800\n\
        -B baud: set Garmin baud rate (300,600,1200,2400,4800,9600,19200) \n\
        -d msg: disable output message (can specify more than one -d option)\n\
        -D: disable all output messages\n\
        -e msg: enable output message (can specify more than one -e option)\n\
        -E: enable all output messages\n\
        -n: disable PPS output\n\
        -p pw: enable PPS output, pw=pulse width in msec (20 to 980)\n\
        device: Name of serial device or pseudo-terminal, e.g. /dev/gps0\n\
        \n\
        If no enable or disable options are entered, then " << argv0 << "\n\
        reads the GPS and returns a status of 0 if the data looks like\n\
        GPS data, or status=1 if data is garbage. This provides a way to check\n\
        from a script that the baud rate is OK.\n\
        \n\
        Returns: 0=success, 1=error\n\
        \n\
        Message types that can be enabled/disabled:\n\
        GPGGA: GPS Fix Data\n\
        time,lat,lon,qual,nsat,hordil,alt,geoidht,diffage,diffid\n\
        GPGSA: Dilution of Position and Active Satellites\n\
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

    char buf[256];
    int l = gps.readUntil(buf,sizeof(buf),'\n');
    if (l == 0)
        throw n_u::IOTimeoutException(gps.getName(),"read");
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
        nc1++;
    }
    string::size_type nc2 = str.find(',',nc1);
    return str.substr(nc1,nc2-nc1);
}

/* static */
string Garmin::substCRNL(const string& str)
{
    return nidas::util::addBackslashSequences(str);
}

bool Garmin::setBaudRateOption()  throw(n_u::IOException)
{
    cout << "setBaudRateOption:" << endl;
    int baudRateIndex = getBaudRateIndex(newBaudRate);
    if (baudRateIndex <= 0) {
        cerr << "ERROR: Incorrect baud rate requested: " << newBaudRate << endl;
        return false;
    }

    ostringstream ost;
    ost << baudRateIndex;
    string baudstr = ost.str();

    ost.str("");
    ost << "$PGRMC,,,,,,,,,," << baudstr << ",,,,\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    // read answer back
    for (int i = 0; i < 2; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            if (instr == outstr) break;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: setBaudRateOption: " << e.what() << endl;
            return false;
        }
    }
    ost.str("");
    ost << "$PGRMCE\r\n";
    outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    for (int i = 0; i < 10; i++) {
        try {
            string instr = readMessage();
            if (instr.substr(0,6) == "$PGRMC") {
                cout << "current $PGRMC settings:" << endl;
                cout << substCRNL(instr) << endl;
                string baud = getStringField(instr,10);
                // cout << "baud=" << baud << endl;
                if (baud != baudstr) {
                    int bi = atoi(baud.c_str());
                    cerr << "ERROR: Active baud rate is " << getBaudRate(bi) << 
                        " (" << baud << "). Power cycle GPS for new rate." << endl;
                    return false;
                }
                return true;
            }
            cout << substCRNL(instr) << endl;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR:setBaudRateOption: " << e.what() << endl;
            break;
        }
    }

    cerr << "ERROR: setBaudRateOption: no $PGRMC message received" << endl;
    return false;
}

/* return 1: PPS disabled, 2: PPS enabled, 0: unknown(error) */
int Garmin::checkPPS(int* pulseWidthPtr) throw(n_u::IOException)
{
    cout << "checkPPS:" << endl;

    int ppsSetting = 0;
    int pwSetting = -1;

    ostringstream ost;
    ost << "$PGRMCE\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    for (int i = 0; i < 2; i++) {
        try {
            string instr = readMessage();
            if (instr.substr(0,6) == "$PGRMC") {
                cout << "current $PGRMC settings:" << endl;
                cout << substCRNL(instr) << endl;
                string pps = getStringField(instr,12);
                // cout << "pps=" << pps << endl;
                istringstream ist(pps);
                ist >> ppsSetting;
                if (ist.fail() || ppsSetting < 1 || ppsSetting > 2) {
                    cerr << "ERROR: Unknown PPS setting: " << pps << endl;
                    ppsSetting = 0;
                }
                pps = getStringField(instr,13);
                ist.str(pps);
                ist.clear();
                ist >> pwSetting;
                if (ist.fail() || pwSetting < 0 || pwSetting > 48) {
                    cerr << "ERROR: Unknown pulse length setting: " << pps << endl;
                    pwSetting = -1;
                }
                *pulseWidthPtr = pwSetting;
                if (ppsSetting == 2)
                    cout << "PPS currently enabled, width="
                        << getPulseWidth(pwSetting) << " msec" << endl;
                else if (ppsSetting == 1)
                    cout << "PPS currently disabled" << endl;
                return ppsSetting;
            }
            cout << substCRNL(instr) << endl;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: checkPPS: " << e.what() << endl;
            break;
        }
    }
    cerr << "ERROR: checkPPS: no $PGRMC message received" << endl;
    *pulseWidthPtr = -1;
    return 0;
}

bool Garmin::enablePPS() throw(n_u::IOException)
{

    int pwSetting;
    int ppsSetting = checkPPS(&pwSetting);

    cout << "enablePPS:" << endl;
    if (ppsSetting == 2 && pwSetting == pulseWidth) return true;

    ostringstream ost;
    ost << "$PGRMC,,,,,,,,,,,,2," << pulseWidth << ",\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    for (int i = 0; i < 10; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            if (instr == outstr) break;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: enablePPS: " << e.what() << endl;
            break;
        }
    }

    ppsSetting = checkPPS(&pwSetting);
    if (ppsSetting == 2) {
        if (pwSetting == pulseWidth) return true;
        // pulse width change should happen immediately, no power cycle req'd
        cerr << "ERROR: PPS width is " << getPulseWidth(pwSetting) <<
            " msec and did not change.\n" << endl;
        return false;
    }
    if (ppsSetting == 1) {
        cerr << "ERROR: Currently PPS is disabled. Power cycle the Garmin to enable PPS." << endl;
        return false;
    }
    return false;
}

bool Garmin::disablePPS() throw(n_u::IOException)
{

    int pwSetting;
    int ppsSetting = checkPPS(&pwSetting);

    cout << "disablePPS:" << endl;
    if (ppsSetting == 1) return true;

    ostringstream ost;
    ost << "$PGRMC,,,,,,,,,,,,1,,\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    for (int i = 0; i < 10; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            if (instr == outstr) break;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: disablePPS:" << e.what() << endl;
            break;
        }
    }

    ppsSetting = checkPPS(&pwSetting);
    if (ppsSetting == 1) return true;
    if (ppsSetting == 2) {
        cerr << "ERROR: Currently PPS is enabled. Power cycle the Garmin to disable PPS." << endl;
        return false;
    }
    return false;
}

bool Garmin::enableMessage(const string& msg) throw(n_u::IOException)
{
    // check if message is already being received.
    if (rescanMessages && !scanMessages(2)) return false;
    set<string>::const_iterator mi =
        currentMessages.find(msg);
    if (mi != currentMessages.end()) return true;

    cout << "enableMessage: " << msg << endl;
    ostringstream ost;
    ost << "$PGRMO," << msg << ",1\r\n";
    string outstr = ost.str();

    for (int j = 0; j < 2; j++) {
        gps.write(outstr.c_str(),outstr.length());

        // may be other messages too
        for (int i = 0; i < 5; i++) {
            try {
                string instr = readMessage();
                cout << substCRNL(instr) << endl;
                // garmin echoed back string
                if (instr == outstr) return true;
                if (instr.length() > 5 && instr.substr(1,5) == msg)
                    return true;
            }
            catch(const n_u::IOTimeoutException& e) {
                cerr << "ERROR: enable " << msg << ": " << e.what() << endl;
                break;
            }
        }
    }
    cerr << "ERROR: message=" << msg << " not read back after being enabled" << endl;
    return false;
}

bool Garmin::disableMessage(const string& msg) throw(n_u::IOException)
{
    cout << "disableMessage: " << msg << endl;

    rescanMessages = true;

    ostringstream ost;
    ost << "$PGRMO," << msg << ",0\r\n";
    string outstr = ost.str();

    gps.write(outstr.c_str(),outstr.length());

    n_u::UTime tmsg;

    for (int i = 0; i < 5 &&
            (n_u::UTime() - tmsg) < (3 * USECS_PER_SEC); i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            // garmin echoed back string
            if (instr == outstr) return true;
            if (instr.length() > 5 && instr.substr(1,5) == msg)
                tmsg = n_u::UTime();
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: disableMessage: " << e.what() << endl;
            return true;
        }
    }
    if ((n_u::UTime() - tmsg) < 2 * USECS_PER_SEC) {
        cerr << "ERROR: message=" << msg << " not disabled successfully" << endl;
        return false;
    }
    return true;
}

bool Garmin::enableAllMessages() throw(n_u::IOException)
{
    cout << "enableAllMessages:" << endl;
    rescanMessages = true;

    ostringstream ost;
    ost << "$PGRMO,,3\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());
    for (int i = 0; i < 2; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            // garmin echoed back string
            if (instr == outstr) return true;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: enableAllMessages: " << e.what() << endl;
            return false;
        }
    }
    return true;
}

bool Garmin::disableAllMessages() throw(n_u::IOException)
{
    cout << "disableAllMessages:" << endl;

    rescanMessages = true;

    ostringstream ost;
    ost << "$PGRMO,,2\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());
    for (int i = 0; i < 20; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            if (instr == outstr) {
                rescanMessages = false;
                currentMessages.clear();
                return true;
            }
        }
        catch(const n_u::IOTimeoutException& e) {
            // actually should have gotten a $PGRM0 echo back
            cerr << "ERROR: disableAllMessages: " << e.what() << endl;
            return false;
        }
    }
    cerr << "ERROR: disableAllMessages not successful" << endl;

    return false;
}

bool Garmin::readInit() throw(n_u::IOException)
{

    cout << "read initialization information (PGRMI)" << endl;

    ostringstream ost;
    ost << "$PGRMIE\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    for (int i = 0; i < 10; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            return true;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: read init information (PGRMI): " << e.what() << endl;
            break;
        }
    }
    return false;
}
bool Garmin::sendInit(float lat, float lon) throw(n_u::IOException)
{
    // this function is untested.

    cout << "set initialization information with PGRMI" << endl;

    ostringstream ost;

    n_u::UTime utnow = n_u::UTime();


    char ns = (lat < 0.0 ? 'S' : 'N');
    lat = fabs(lat);
    int latdeg = (int)lat;
    float latmin = fmodf(lat,1.0) * 60.0;

    char ew = (lon < 0.0 ? 'W' : 'E');
    lon = fabs(lon);
    int londeg = (int)lon;
    float lonmin = fmodf(lon,1.0) * 60.0;
    ost << "$PGRMI," <<
        setw(2) << setfill('0') << latdeg <<
        setprecision(3) << setw(8) << setfill('0') << latmin << ',' << ns << ',' <<
        setw(3) << setfill('0') << londeg << ',' <<
        setprecision(3) << setw(9) << setfill('0') << lonmin << ',' << ew << ',' <<
        utnow.format(true,"%02d%02m%02y,%02H%02M%02S") << ',' <<
        'R' << "\r\n";
    string outstr = ost.str();
    gps.write(outstr.c_str(),outstr.length());

    for (int i = 0; i < 10; i++) {
        try {
            string instr = readMessage();
            cout << substCRNL(instr) << endl;
            if (instr == outstr) return true;
        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "ERROR: set init information (PGRMI): " << e.what() << endl;
            break;
        }
    }
    return false;
}

/* returns true if data looks like GPS data, or a timeout. */
bool Garmin::scanMessages(int seconds) throw(n_u::IOException)
{
    bool OK = true;
    cout << "scanMessages: " << endl;
    n_u::UTime tthen = n_u::UTime() + seconds * USECS_PER_SEC;

    currentMessages.clear();

    while (n_u::UTime() < tthen) {
        try {
            string str = readMessage();
            cout << substCRNL(str) << endl;
            if (str.length() > 5 && str[0] == '$') {
                currentMessages.insert(str.substr(1,5));
                OK = true;
            }
            else OK = false;

        }
        catch(const n_u::IOTimeoutException& e) {
            cerr << "scanMessages: " << e.what() << endl;
        }
    }
    if (!OK) cerr << "Data doesn't look like Garmin GPS data. Wrong baud rate?" << endl;
    rescanMessages = false;
    return OK;
}
int Garmin::run()
{
    try {

        gps.setName(device);
        n_u::Termios& tio = gps.termios();

        tio.setBaudRate(baudRate);
        tio.setRaw(true);
        tio.setRawTimeout(20);
        tio.setRawLength(0);

        gps.open(O_RDWR);

        FD_ZERO(&readfds);
        FD_SET(gps.getFd(),&readfds);

        maxfd = gps.getFd() + 1;

        // read messages for 2 seconds
        if (!scanMessages(2)) return 1;

        if (newBaudRate >= 0) setBaudRateOption();

        if (disableAllMsg) disableAllMessages();
        if (enableAllMsg) enableAllMessages();

        set<string>::const_iterator mi = toDisable.begin();
        for ( ; mi != toDisable.end(); ++mi) 
            if (!disableMessage(*mi)) return 1;

        mi = toEnable.begin();
        for ( ; mi != toEnable.end(); ++mi) 
            if (!enableMessage(*mi)) return 1;

        if (ppsDisable && !disablePPS()) return 1;
        else if (pulseWidth >= 0 && !enablePPS()) return 1;

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

