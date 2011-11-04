// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

 Command line app for controlling Diamond Systems IR104 relays.

*/

#include <iostream>
#include <iomanip>

#include <nidas/util/Logger.h>
#include <nidas/dynld/IR104_Relays.h>

using namespace std;
using namespace nidas::dynld;

namespace n_u = nidas::util;

static int usage(const char* argv0)
{
    cerr << "Usage: \n" <<
        argv0 << " device i [x] ...\n" <<
        argv0 << " device [0xHHHH]\n\
device: typically /dev/ir104_0\n\
i: output channel, 0-19\n\
x: value, 0 or 1\n\
    If one channel is specified, with no value, return current input value\n\
0xHHHHH: hexadecimal value of relay bit values, using hex digits 0-9,a-f,A-F.\n\
    For example, 0x0 clears all relays, 0xfffff sets all\n\
    0xa3 sets relays 0,1,5 and 7 on, others off\n\
    If no 0xHHHHH, query the current settings and report back in hex" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    n_u::LogScheme ls = n_u::Logger::getInstance()->getScheme();
    ls.clearConfigs();
    n_u::LogConfig lc;
    lc.level = n_u::LOGGER_WARNING;
    ls.addConfig(lc);
    n_u::Logger::getInstance()->setScheme(ls);

    IR104_Relays dout;

    if (argc < 2) return usage(argv[0]);

    dout.setDeviceName(argv[1]);

    dout.open(O_RDWR);

    int nout = dout.getNumOutputs();
    // int nin = dout.getNumInputs();
    //
    if (argc == 2) {
        n_u::BitArray vals = dout.getOutputs();
        cout << "0x" << hex << setfill('0') << setw(5) << vals.getBits(0,nout) << endl;
        return 0;
    }

    // If one arg, it could be 0xHHHH to set all relays, or a decimal number
    // to query one relay
    if (argc == 3) {
        if (!strncmp(argv[2],"0x",2)) {
            char* cp;
            unsigned int rval = (unsigned int)strtol(argv[2],&cp,0);
            if (cp <= argv[2]+2) return usage(argv[0]);
            n_u::BitArray which(nout);
            which.setBits(1);
            n_u::BitArray vals(nout);
            vals.setBits(0,nout,rval);
            dout.setOutputs(which,vals);
            return 0;
        }
        int io = atoi(argv[2]);
        if (io < 0 || io >= nout) {
            cerr << "channel number " << io << " is out of range: " << 0 << '-' << (nout-1) << endl;
            return usage(argv[0]);
        }
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

