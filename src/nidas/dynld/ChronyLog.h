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

#ifndef NIDAS_DYNLD_CHRONYLOG_H
#define NIDAS_DYNLD_CHRONYLOG_H

#include "WatchedFileSensor.h"
#include <nidas/core/ChronyStatus.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A WatchedFileSensor for a chrony tracking log, with code to
 * grab values for the NTP stratum and time offset, and supporting
 * the printChronyStatus() method to output HTML for monitoring those
 * values with the NIDAS status_listener.
 *
 * If you don't need this printChronyStatus() capability you can just use
 * a WatchedFileSensor on the chrony tracking log.
 *
 * By default this ChronyLog looks for variables in its output
 * sample tags with names beginning with "Stratum" and "Timeoffset",
 * saving the associated sample id and the variable's offset in the sample.
 *
 * In the process method(), after calling WatchedFileSensor::process()
 * on the raw sample, the stratum and time offset are saved from the
 * processed sample, so that their values can be printed in with
 * printChronyStatus().
 *
 * If those variables have different names, then add <parameter>s to the <sensor>
 * indicating what the names are. For example, if the variables are
 * named "NTPstratum" and "NTPoffset", specify these parameters so that
 * the values can be extracted from the processed samples:
 *  <parameter name="Stratum" type="string" value="NTPstratum"/>
 *  <parameter name="Timeoffset" type="string" value="NTPoffset"/>
 */
class ChronyLog: public WatchedFileSensor, public ChronyStatusNode
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    ChronyLog();

    /**
     * Scan the configuration for parameters indicating
     * the names of the Stratum and TimeOffset variables.
     * Save the sample id and variable indices of the stratum
     * and offset.
     */
    void init();

    /**
     * Call WatchedFileSensor::process, then check for
     * processed values for stratum and time offset in the results.
     */
    bool process(const Sample* samp, std::list<const Sample*>& results) throw();

    /**
     * Print out HTML for status of the NTP stratum and time offset.
     * Note these values will only have meaningful values when samples
     * are processed, and so this is not typically called on a DSM,
     * but in the dsm_server.
     */
    void printChronyStatus(std::ostream& ostr) throw();

private:

    dsm_time_t _lastSampleTime;

    struct statusVariable {
        statusVariable(): name(), id(0), varindex(0) {}
        std::string name;
        dsm_sample_id_t id;
        unsigned int varindex;
    };

    struct statusVariable _statusVariables[2];

    float _stratum;

    float _toffset;

    /**
     * Scale factor to convert _toffset to usecs.
     */
    float _toffsetToUsecs;

    /** No copying */
    ChronyLog(const ChronyLog&);

    /** No assignment */
    ChronyLog& operator=(const ChronyLog&);

};

}}	// namespace nidas namespace dynld

#endif
#endif  // HAVE_SYS_INOTIFY_H
