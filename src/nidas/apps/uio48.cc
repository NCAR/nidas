/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
/*

 Command line app for controlling UIO48 digital I/O pins.

*/

#include <iostream>

#include <nidas/dynld/Uio48Sensor.h>

using namespace std;
using namespace nidas::dynld;

namespace n_u = nidas::util;

static int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " device [i [x] ...]\n\
device: typically /dev/uio48a\n\
i: pin, 0-23\n\
x: value, 0 or 1\n\
If one pin number is specified, with no value, return current input value" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    Uio48 dio;

    int ret = 0;

    if (argc < 2) return usage(argv[0]);

    int iarg = 1;

    string devname = "/dev/uio48a";
    if (argv[iarg][0] == '/') devname = argv[iarg++];

    try {

        dio.open(devname.c_str());

        int npins = dio.getNumPins();

        // query state of digital pin.
        if (argc - iarg == 1) {
            int io = atoi(argv[iarg++]);
            if (io < 0 || io >= npins) {
                cerr << "pin number " << io << " is out of range: " << 0 << '-' << (npins-1) << endl;
                return usage(argv[0]);
            }
            // n_u::BitArray vals = dio.getInputs();
            n_u::BitArray vals = dio.getPins();
            cout << vals.getBit(io) << endl;
            return 0;
        }

        // cout << "num pins=" << npins << endl;
        // cout << "num inputs=" << nin << endl;

        n_u::BitArray which(npins);
        n_u::BitArray vals(npins);
        for ( ; iarg < argc - 1; ) {
            int io = atoi(argv[iarg++]);

            if (io < 0 || io >= npins) {
                cerr << "pin number " << io << " is out of range: " << 0 << '-' << (npins-1) << endl;
                return usage(argv[0]);
            }
            int val = atoi(argv[iarg++]);
            which.setBit(io,1);
            vals.setBit(io,val!=0);
            // cout << "setting DOUT pin " << io << " to " << val << endl;
        }
        dio.setPins(which,vals);
        return 0;
    }
    catch (const n_u::IOException & e) {
        cerr << e.what() << endl;
        ret = 1;
    }
    return ret;
}

