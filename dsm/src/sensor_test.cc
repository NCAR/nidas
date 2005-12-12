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
};

Runstring::Runstring(int argc, char** argv): type(UNKNOWN)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
										
    while ((opt_char = getopt(argc, argv, "dmp")) != -1) {
	switch (opt_char) {
	case 'm':
	    type = MENSOR_6100;
	    break;
	case 'p':
	    type = PARO_1000;
	    break;
	case 'd':
	    type = BUCK_DP;
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
  device: Name of serial device, e.g. /dev/ttyS1\n\
" << endl;
    exit(1);
}

int main(int argc, char** argv)
{
  
    Runstring rstr(argc,argv);

    atdTermio::SerialPort p(rstr.device);
    switch (rstr.type) {
    case Runstring::MENSOR_6100:
	p.setBaudRate(57600);
	p.iflag() = ICRNL;
	p.oflag() = OPOST;
	p.lflag() = ICANON;
	break;
    case Runstring::PARO_1000:
	p.setBaudRate(57600);
	p.iflag() = 0;
	p.oflag() = OPOST;
	p.lflag() = ICANON;
	break;
    case Runstring::BUCK_DP:
	p.setBaudRate(9600);
	p.iflag() = 0;
	p.oflag() = OPOST;
	p.lflag() = ICANON;
	break;
    case Runstring::UNKNOWN:
        return 1;
    }

    char inbuf[128];
    char outbuf[128];
    int iout = 0;

    string promptStrings[] = { "#1?\n", "*0100P3\r\n",""};
    const char* dataFormats[] = { "1%f\r\n" , "*0001%f\r\n", 
    	"14354,-14.23,0,0,-56,0, 33.00,05/08/2003, 17:47:08\r\n"};

    try {
	p.open(O_RDWR);

	for (;;) {
	    if (promptStrings[rstr.type].length() > 0) {
		int l = p.readline(inbuf,sizeof(inbuf));
		inbuf[l] = '\0';

		if (!strcmp(inbuf,promptStrings[rstr.type].c_str())) {
		    sprintf(outbuf,dataFormats[rstr.type],1000.0);
		    p.write(outbuf,strlen(outbuf));
		}
		else cerr << "unrecognized prompt: \"" << inbuf << "\"" << endl;
	    }
	    else {
	        sleep(1);
		if (iout++ < 10) {
		    // simulate empty outputs
		    strcpy(outbuf,"\n");
		    p.write(outbuf,strlen(outbuf));
		}
		else {
		    strcpy(outbuf,dataFormats[rstr.type]);
		    p.write(outbuf,strlen(outbuf));
		}
	    }
	}
    }
    catch(atdUtil::IOException& ioe) {
	cerr << ioe.what() << endl;
    }
}
