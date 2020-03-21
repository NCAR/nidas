// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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

#include "UDPArincStatus.h"

#include <nidas/util/Logger.h>


using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, UDPArincStatus);


UDPArincStatus::UDPArincStatus()
{

}


UDPArincStatus::~UDPArincStatus()
{

}


void UDPArincStatus::extractStatus(const char *msg, int len)
{
    const char *p = &msg[7];    // skip STATUS, keyword
    int cnt = 0;    // comma count
    char s[len];

    for (int i = 7; i < len && cnt < 4; ++i) {
        if (*p++ == ',')
            ++cnt;
        if (cnt == 1) {
            len -= (p-msg);
            ::memcpy(s, p, len); s[len]=0;
            configStatus["PBIT"] = atoi(s);
        }
        if (cnt == 3) {
            len -= (p-msg);
            ::memcpy(s, p, len); s[len]=0;
            configStatus["PPS"] = atoi(s);
        }
    }
}


void UDPArincStatus::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
        ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
        return;
    }


    ostr << "<td align=left>";
    bool firstPass = true;
    for (map<string,int>::iterator it = configStatus.begin(); it != configStatus.end(); ++it)
    {
        bool red = false;
        if (!firstPass) ostr << ',';

        ostr << "<font";
        if ((it->first).compare("PBIT") == 0) {
            if (it->second != 0)
                red = true;
        }
        else
        if ((it->first).compare("PPS") == 0) {
            if (it->second != 0)
                red = true;
        }
        else
        if (it->second == false)
            red = true;

        if (red) ostr << " color=red";
        ostr << "><b>" << it->first << "</b></font>";
        firstPass = false;
    }
    ostr << "</td>";
}

