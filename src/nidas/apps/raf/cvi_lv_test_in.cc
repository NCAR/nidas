/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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

        for (;;) {
            n_u::Socket* sock = servsock.accept();
            cerr << "connection from " <<
                sock->getRemoteSocketAddress().toString() << endl;

            try {
                char buffer[2048];
                for (;;) {
                    size_t inlen = sock->recv(buffer,sizeof(buffer));
                    buffer[inlen] = 0;
                    cout << buffer;
                }
            }
            catch (const n_u::EOFException&e)
            {
                cerr << e.what() << endl;
                sock->close();
            }
        }
    }
    catch (const n_u::IOException&e)
    {
        cerr << e.what() << endl;
        return 1;
    }
}
