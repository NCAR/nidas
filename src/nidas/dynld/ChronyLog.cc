// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>

#ifdef HAVE_SYS_INOTIFY_H

#include "ChronyLog.h"

#include <nidas/core/Variable.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(ChronyLog)

ChronyLog::ChronyLog():
    _lastSampleTime(0),
    _statusVariables(),
    _stratum(floatNAN),
    _toffset(floatNAN),
    _toffsetToUsecs(1.0)
{
    _statusVariables[0].name = "Stratum";
    _statusVariables[1].name = "Timeoffset";
}

void ChronyLog::init()
{

    WatchedFileSensor::init();

    /* Look for string parameters, matching the initial names in
     * in _statusVariables ("Stratum", "Timeoffset"). The values
     * of those parameters are the actual variable name in the XML.
     * For example, if the actual name for the Timeoffset variable is
     * "toff", then add this <parameter> to the <sensor>:
     *      <parameter type="string" name="Timeoffset" value="toff"/>
     */

    if (_statusVariables[0].id == 0) {
        for (unsigned int i = 0;
                i < sizeof(_statusVariables) / sizeof(_statusVariables[0]); i++) {
            const Parameter* param = getParameter(_statusVariables[i].name);
            if (!param) continue;
            if (param->getLength() != 1) 
                throw nidas::util::InvalidParameterException(getName(),
                    "parameter", param->getName() + " should be of length 1");
            if (param->getType() != Parameter::STRING_PARAM) 
                throw nidas::util::InvalidParameterException(getName(),
                    "parameter", param->getName() + " is not a string");
            _statusVariables[i].name = param->getStringValue(0);
        }

        for (SampleTagIterator si = getSampleTagIterator(); si.hasNext(); ) {
            const SampleTag* stag = si.next();

            VariableIterator vi = stag->getVariableIterator();
            for (int iv = 0; vi.hasNext(); iv++) {
                const Variable* var = vi.next();
                const string& vname = var->getName();
                for (unsigned int i = 0;
                    i < sizeof(_statusVariables) / sizeof(_statusVariables[0]); i++) {
                    // use rfind at the beginning of vname to check if it
                    // starts with _statusVariables[i].name
                    if (vname.rfind(_statusVariables[i].name,0) == 0) {
                        _statusVariables[i].id = stag->getId();
                        _statusVariables[i].varindex = iv;
                        // time offset, check units, if they appear to be seconds, convert
                        // to micro-seconds.
                        if (i == 1) {
                            string units = var->getUnits();
                            if (var->getConverter()) units = var->getConverter()->getUnits();
                            if (units[0] == 's') _toffsetToUsecs = 1.e+6;
                        }
                    }
                }
            }
        }
    }
}

bool ChronyLog::process(const Sample* samp, std::list<const Sample*>& results) throw()
{
    std::list<const Sample*> vane;
    WatchedFileSensor::process(samp,results);
    if (results.empty()) return false;

    // save _stratum, _toffset and _lastSampleTime for use in printChronyStatus()
    std::list<const Sample*>::const_iterator si = results.begin();
    for ( ; si != results.end(); ++si) {
        const Sample* csamp = *si;
        if (csamp->getId() == _statusVariables[0].id &&
                csamp->getDataLength() > _statusVariables[0].varindex) {
            _stratum = csamp->getDataValue(_statusVariables[0].varindex);
            _lastSampleTime = csamp->getTimeTag();
        }
        if (csamp->getId() == _statusVariables[1].id &&
                csamp->getDataLength() > _statusVariables[1].varindex) {
            _toffset = csamp->getDataValue(_statusVariables[1].varindex) * _toffsetToUsecs;
        }
    }

    // Use TEST_PRINT_CHRONY_STATUS for a quick check of output html without having
    // to start a dsm_server and status_listener.
// #define TEST_PRINT_CHRONY_STATUS
#ifdef TEST_PRINT_CHRONY_STATUS
    static unsigned int count = 0;

    if (count++ < 10) {
        ostringstream ostr;
        if (count == 1) printChronyHeader(ostr);
        printChronyStatus(ostr);
        if (count == 10) printChronyTrailer(ostr);
        cerr << ostr.str() << endl;
    }

#endif
    return true;
}

void ChronyLog::printChronyStatus(std::ostream& ostr) throw()
{

    // chronyZebra is defined in nidas/core/ChronyStatus.h
    string oe(chronyZebra ? "odd" : "even");

    chronyZebra = !chronyZebra;

    ostr << "<tr class=" << oe << "><td>" << getDSMName() << "</td>";
    /*
     * stratum: print value: "unk" if nan, print in red if > 2
     */
    if (isnan(_stratum))
        ostr << "<td><font color=red><b>unk</b></font>";
    else {
        ostr << fixed << setprecision(0);
        if (_stratum > 2)
            ostr << "<td><font color=red><b>" << _stratum << "</b></font>";
        else
            ostr << "<td>" << _stratum;
    }
    ostr << "</td>";

    /*
     * toffset: print value: "unk" if nan, in red if > 0.5 sec, in orange if > 0.01 sec.
     * These checks are a first guess for a quick status display for techs, might need
     * to be altered.
     */
    if (isnan(_toffset))
        ostr << "<td><font color=red><b>unk</b></font>";
    else {
        ostr << fixed;
        if (_toffset > 5.e+5) {
            // greater than half second, print in red
            ostr << "<td><font color=red><b>" << setprecision(0) << _toffset << "</b></font>";
        }
        else if (_toffset > 1.e+4) {
            // greater than 10 milliseconds, print in orange
            ostr << "<td><font color=orange><b>" << setprecision(0) << _toffset << "</b></font>";
        }
        else {
            ostr << setprecision(3);
            ostr << "<td>" << _toffset;
        }
    }
    ostr << "</td>";

    int age = 9999;
    if (_lastSampleTime > 0LL) age = (n_u::UTime().toUsecs() - _lastSampleTime) / USECS_PER_SEC;
    age = std::min(9999, age);

    if (age > 60)
        ostr << "<td><font color=red><b>" << age << "</b></font>";
    else
        ostr << "<td>" << age;

    ostr << "</td></tr>";
}

#endif  // HAVE_SYS_INOTIFY_H
