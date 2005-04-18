/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#include <atdUtil/Socket.h>
#include <Datagrams.h>

#include <iostream>

using namespace dsm;
using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);
    static void usage(const char* argv0);
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
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << endl;
    exit(1);
}

int main(int argc, char** argv)
{

    Runstring rstr(argc,argv);

    atdUtil::MulticastSocket msock(DSM_MULTICAST_STATUS_PORT);
    msock.joinGroup(atdUtil::Inet4Address::getByName(DSM_MULTICAST_ADDR));
    char buf[8192];
    atdUtil::Inet4SocketAddress from;
    for (;;) {
        size_t l = msock.recvfrom(buf,sizeof(buf),0,from);
	cout << from.toString() << endl;
	cout << buf;
    }
}


