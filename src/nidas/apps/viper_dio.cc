/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

 Check digital out device.

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
i: output channel, 0-3\n\
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

