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

 Set one or more analog out voltages on a Diamond Systems Corp DMM AT card.

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

