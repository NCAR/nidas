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

 Command line app for controlling DIO pins on a Viper.

*/

#include <iostream>

#include <nidas/dynld/ViperDIO.h>

using namespace std;
using namespace nidas::dynld;

namespace n_u = nidas::util;

static int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " device [i [x] ...]\n\
device: typically /dev/viper_dio0\n\
i: output channel, 0-7\n\
x: value, 0 or 1\n\
If one channel is specified, with no value, return current input value" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    ViperDIO dout;

    if (argc < 2) return usage(argv[0]);

    dout.setDeviceName(argv[1]);

    dout.open();

    int nout = dout.getNumOutputs();
    // int nin = dout.getNumInputs();

    // query state of digital out pin.
    if (argc == 3) {
        int io = atoi(argv[2]);
        if (io < 0 || io >= nout) {
            cerr << "channel number " << io << " is out of range: " << 0 << '-' << (nout-1) << endl;
            return usage(argv[0]);
        }
        // n_u::BitArray vals = dout.getInputs();
        n_u::BitArray vals = dout.getOutputs();
        cout << vals.getBit(io) << endl;
        return 0;
    }

    // cout << "num outputs=" << nout << endl;
    // cout << "num inputs=" << nin << endl;

    n_u::BitArray which(nout);
    n_u::BitArray vals(nout);
    for (int i = 2; i < argc - 1;  ) {
        int io = atoi(argv[i++]);

        if (io < 0 || io >= nout) {
            cerr << "channel number " << io << " is out of range: " << 0 << '-' << (nout-1) << endl;
            return usage(argv[0]);
        }
        int val = atoi(argv[i++]);
        which.setBit(io,1);
        vals.setBit(io,val!=0);
        // cout << "setting DOUT pin " << io << " to " << val << endl;
    }
    dout.setOutputs(which,vals);
    return 0;
}

