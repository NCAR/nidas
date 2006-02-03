/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <fcntl.h>
#include <iostream>
#include <string.h>

#include <atdTermio/SerialPort.h>

using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);
    static void usage(const char* argv0);
    string device;
    enum sens_type { MENSOR_6100, PARO_1000, BUCK_DP, UNKNOWN } type;
    bool openpty;
};

Runstring::Runstring(int argc, char** argv): type(UNKNOWN)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    openpty = false;

    while ((opt_char = getopt(argc, argv, "dmpt")) != -1) {
	switch (opt_char) {
	case 'd':
	    type = BUCK_DP;
	    break;
	case 'm':
	    type = MENSOR_6100;
	    break;
	case 'p':
	    type = PARO_1000;
	    break;
	case 't':
	    openpty = true;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) usage(argv[0]);
    if (type == UNKNOWN) usage(argv[0]);
    if (optind != argc) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p | -m]  device\n\
  -p: simulate ParoScientific DigiQuartz 1000\n\
  -m: simulate Mensor 6100\n\
  -d: simulate Buck dewpointer\n\
  -t: open pseudo-terminal device\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/ttyS1, or /tmp/pty/dev0\n\
" << endl;
    exit(1);
}

int createPtyLink(const std::string& link) throw(atdUtil::IOException)
{
    int fd;
    const char* ptmx = "/dev/ptmx";

    if ((fd = open(ptmx,O_RDWR)) < 0) 
    	throw atdUtil::IOException(ptmx,"open",errno);

    char* slave = ptsname(fd);
    if (!slave) throw atdUtil::IOException(ptmx,"ptsname",errno);

    cerr << "slave pty=" << slave << endl;

    if (grantpt(fd) < 0) throw atdUtil::IOException(ptmx,"grantpt",errno);
    if (unlockpt(fd) < 0) throw atdUtil::IOException(ptmx,"unlockpt",errno);

    struct stat linkstat;
    if (lstat(link.c_str(),&linkstat) < 0) {
        if (errno != ENOENT)
		throw atdUtil::IOException(link,"stat",errno);
    }
    else {
        if (S_ISLNK(linkstat.st_mode)) {
	    cerr << link << " is a symbolic link, deleting" << endl;
	    if (unlink(link.c_str()) < 0)
		throw atdUtil::IOException(link,"unlink",errno);
	}
	else
	    throw atdUtil::IOException(link,
	    	"exists and is not a symbolic link","");

    }
    if (symlink(slave,link.c_str()) < 0)
	throw atdUtil::IOException(link,"symlink",errno);
    return fd;
}

int main(int argc, char** argv)
{
  
    Runstring rstr(argc,argv);

    try {

	auto_ptr<atdTermio::SerialPort> port;

	if (rstr.openpty) {
	    int fd = createPtyLink(rstr.device);
	    port.reset(new atdTermio::SerialPort("/dev/ptmx",fd));
	}
	else port.reset(new atdTermio::SerialPort(rstr.device));

	switch (rstr.type) {
	case Runstring::MENSOR_6100:
	    port->setBaudRate(57600);
	    port->iflag() = ICRNL;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    break;
	case Runstring::PARO_1000:
	    port->setBaudRate(57600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    break;
	case Runstring::BUCK_DP:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    break;
	case Runstring::UNKNOWN:
	    return 1;
	}

	char inbuf[128];
	char outbuf[128];
	int iout = 0;
	struct timespec sleep10msec = { 0, 10000000 };
	struct timespec sleep100msec = { 0, 100000000 };

	string promptStrings[] = { "#1?\n", "",""};
	const char* dataFormats[] = { "1%f\r\n" , "*0001%f\r\n", 
	    "14354,-14.23,0,0,-56,0, 33.00,05/08/2003, 17:47:08\r\n"};

	if (!rstr.openpty) port->open(O_RDWR);

	for (;;) {
	    if (promptStrings[rstr.type].length() > 0) {
		int l = port->readline(inbuf,sizeof(inbuf));
		inbuf[l] = '\0';

		if (!strcmp(inbuf,promptStrings[rstr.type].c_str())) {
		    sprintf(outbuf,dataFormats[rstr.type],1000.0);
		    port->write(outbuf,strlen(outbuf));
		}
		else cerr << "unrecognized prompt: \"" << inbuf << "\"" << endl;
	    }
	    else {
	        nanosleep(&sleep100msec,0);
		cerr << "writing" << endl;
		if (iout++ < 10) {
		    // simulate empty outputs
		    strcpy(outbuf,"\n");
		    port->write(outbuf,strlen(outbuf));
		}
		else {
		    strcpy(outbuf,dataFormats[rstr.type]);
		    port->write(outbuf,strlen(outbuf));
		}
	    }
	}
    }
    catch(atdUtil::IOException& ioe) {
	cerr << ioe.what() << endl;
    }
}
