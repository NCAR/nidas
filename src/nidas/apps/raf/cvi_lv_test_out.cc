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
#include <fstream>
#include <sstream>

using namespace std;

namespace n_u = nidas::util;

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " file host port" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 4)
      return usage(argv[0]);

    const char* file = argv[1];

    const char* host = argv[2];

    istringstream ist(argv[3]);
    int port;

    ist >> port;
    if (ist.fail()) return usage(argv[0]);

    ifstream fin(file);
    if (fin.fail())
        throw n_u::IOException(file,"open",errno);
    char buf[1024];
    fin.getline(buf,sizeof(buf),'\n');
    fin.getline(buf,sizeof(buf),'\n');

    for (;;) {

        n_u::Socket sock;
        try {
            sock.connect(host,port);
            cerr << "connected: " << sock.getRemoteSocketAddress().toString() << endl;

            for (;;) {

                fin.getline(buf,sizeof(buf),'\n');
                if (fin.eof()) {
                    fin.seekg(0,ios_base::beg);
                    fin.getline(buf,sizeof(buf),'\n');
                    fin.getline(buf,sizeof(buf),'\n');
                    continue;
                }
                if (fin.fail()) throw n_u::IOException(file,"read",errno);

                buf[fin.gcount()-1] = '\n';
                sock.send(buf,fin.gcount());
                sleep(1);
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

