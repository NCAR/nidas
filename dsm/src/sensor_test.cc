/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <fcntl.h>
#include <iostream>
#include <string.h>

#include <atdTermio/SerialPort.h>

using namespace std;

int main(int argc, char** argv)
{
  
    const char* dev = "/dev/ttyS1";
    if (argc > 1) dev = argv[1];

    atdTermio::SerialPort p(dev);
    p.setBaudRate(115200);
    p.setRaw(true);
    p.setRawLength(6);
    char buf[1024];
    char* wp = buf;
    char* rp = buf;
    char* ep = buf + sizeof(buf) - 1;	// save room to '\0' terminate
    char* tp;

    int good = 0;
    int bad = 0;

    try {
	p.open(O_RDWR);

	for (;;) {
	    int l = p.read(wp,ep - wp);
	    // cerr << "l=" << l << endl;
	    wp += l;
	    *wp = '\0';
	    for (; rp <= wp - 6;) {
		if (!strncmp(rp,"hitme\n",6)) {
		    // cerr << "received hitme, writing fake data" << endl;
		    p.write("xxx 1.23 ff\n",12);
		    good++;
		    rp += 6;
		} else {
		    cerr << "error, rp=" << rp << " l=" << l << endl;
		    bad++;
		    rp++;
		}

		if (!((good + bad) % 1000)) cerr << dec << 
			"good=" << good << " bad=" << bad << endl;
	    }
	    // shift down
	    if (rp > buf) {
		for (tp = buf; rp < wp; ) *tp++ = *rp++;
		wp = tp;
		*wp = '\0';
		rp = buf;
	    }
	}
    }
    catch(atdUtil::IOException& ioe) {
	cerr << ioe.what() << endl;
    }
}
