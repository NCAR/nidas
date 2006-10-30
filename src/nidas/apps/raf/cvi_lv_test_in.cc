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
    cerr << "Usage: " << argv0 << " port" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
      return usage(argv[0]);

    istringstream ist(argv[1]);
    int port;

    ist >> port;
    if (ist.fail()) return usage(argv[0]);

    try {
        n_u::ServerSocket servsock(port);

        n_u::Socket* sock = servsock.accept();
        cerr << "connection from " <<
            sock->getRemoteSocketAddress().toString() << endl;

        for (;;) {

            char buffer[1024];

            size_t inlen = sock->recv(buffer,sizeof(buffer));
            buffer[inlen] = 0;

            cout << "buffer=" << buffer;
        }
    }
    catch (const n_u::IOException&e)
    {
        cerr << e.what() << endl;
        return 1;
    }
}
