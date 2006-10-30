/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

/**
 * Initial little test program for testing communications over
 * a socket between Unix and LabView.
 */

#include <nidas/util/Socket.h>

#include <iostream>
#include <sstream>

using namespace std;

namespace n_u = nidas::util;

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " host port" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 3)
      return usage(argv[0]);

    const char* host = argv[1];

    istringstream ist(argv[2]);
    int port;

    ist >> port;
    if (ist.fail()) return usage(argv[0]);

    for (;;) {

        n_u::Socket sock;
        try {
            sock.connect(host,port);
            cerr << "connected: " << sock.getRemoteSocketAddress().toString() << endl;

            ostringstream ost;

            for (int j = 0; ; j++) {

                for (int i = 0; i < 4; i++) ost << i+j << ' ';
                ost << endl;

                string outstr = ost.str();
                sock.send(outstr.c_str(),outstr.length());
                sleep(1);

                ost.str("");
            }
        }
        catch(const n_u::UnknownHostException&e )
        {
            cerr << e.what() << endl;
            return 1;
        }
        catch (const n_u::IOException&e)
        {
            cerr << e.what() << endl;
            sock.close();
            sleep(5);
        }
    }
}

