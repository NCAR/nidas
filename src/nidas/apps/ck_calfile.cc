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

#include <iostream>
#include <nidas/core/CalFile.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " cal_path cal_file" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 3)
      return usage(argv[0]);

    CalFile cf;

    cf.setPath(argv[1]);
    cf.setFile(argv[2]);

    float data[60];
    n_u::UTime tlast((time_t)0);

    try {
        for (;;) {
            n_u::UTime calTime((long long)0);
            if (cf.eof()) break;
            int n = cf.readCF(calTime, data,sizeof data/sizeof data[0]);
            cout << calTime.format(true,"%Y %m %d %H:%M:%S.%3f %Z ");
            for (int i = 0; i < n; i++) cout << data[i] << ' ';
            cout << endl;
            if (calTime < tlast) cerr << "backwards time at " <<
                calTime.format(true,"%Y %m %d %H:%M:%S.%3f %Z ") <<
                " nline=" << cf.getLineNumber() << endl;
            tlast = calTime;
        }
    }
    catch (const n_u::ParseException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (n_u::EOFException& ioe) {
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
        return 1;
    }
    return 0;
}
