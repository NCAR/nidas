/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

 Check analog out device.

*/

#include <iostream>

#include <nidas/dynld/DSC_AnalogOut.h>

using namespace std;
using namespace nidas::dynld;

int main(int argc, char** argv)
{
    DSC_AnalogOut aout;

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " device [i v ...]" << endl;
        cerr << "   device: typically /dev/dmmat_d2a0" << endl;
        cerr << "   i: analog output channel, 0-N" << endl;
        cerr << "   v: voltage, 0.0-5.0" << endl;
        return 1;
    }

    aout.setDeviceName(argv[1]);

    aout.open();

    cerr << "num outputs=" << aout.getNumOutputs() << endl;
    for (int i = 0; i < aout.getNumOutputs(); i++) {
        cerr << "min voltage " << i << " = " << aout.getMinVoltage(i) << endl;
        cerr << "max voltage " << i << " = " << aout.getMaxVoltage(i) << endl;
    }

    vector<int> which;
    vector<float> volts;
    for (int i = 2; i < argc - 1;  ) {
        int io = atoi(argv[i++]);
        float v = atof(argv[i++]);
        which.push_back(io);
        volts.push_back(v);
        cerr << "setting VOUT pin " << io << " to " << v << " V" << endl;
        // aout.setVoltage(io,v);
    }
    aout.setVoltages(which,volts);
}

