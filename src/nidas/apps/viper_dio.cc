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

int main(int argc, char** argv)
{
    ViperDIO dout;

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " device [i x ...]" << endl;
        cerr << "   device: typically /dev/viper_dio0" << endl;
        cerr << "   i: output channel, 0-3" << endl;
        cerr << "   x: value, 0 or 1" << endl;
        return 1;
    }

    dout.setDeviceName(argv[1]);

    dout.open();

    int nout = dout.getNumOutputs();
    int nin = dout.getNumInputs();

    cerr << "num outputs=" << nout << endl;
    cerr << "num inputs=" << nin << endl;


    n_u::BitArray which(nout);
    n_u::BitArray vals(nout);
    for (int i = 2; i < argc - 1;  ) {
        int io = atoi(argv[i++]);
        int v = atoi(argv[i++]);
        which.setBit(io,1);
        vals.setBit(io,v!=0);
        cerr << "setting DOUT pin " << io << " to " << v << endl;
    }
    dout.setOutputs(which,vals);
}

