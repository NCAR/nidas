// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2013, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDIS_DYNLD_GPS_NOVATEL_SERIAL_H
#define NIDIS_DYNLD_GPS_NOVATEL_SERIAL_H

#include <nidas/dynld/GPS_NMEA_Serial.h>

namespace nidas { namespace dynld {

/**
 * A class for reading Novatel records from a GPS.  The process() method parses
 * BESTPOS and BESTVEL Novatel messages and generates double precision floating
 * point samples.
 */
class GPS_Novatel_Serial : public GPS_NMEA_Serial
{
public:

    GPS_Novatel_Serial();

    void addSampleTag(SampleTag* stag)
        throw(nidas::util::InvalidParameterException);

    /**
     * Virtual method that is called to convert a raw sample containing
     * an ASCII NMEA message to a processed floating point sample.
     * These processed samples contain double precision rather than
     * single precision values because the latitude and longitude
     * reported by GPS's may have more than 7 digits of precision.
     */
    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

    /**
     * Calculate the checksum of the Novatel message and return a logical
     * indicating whether it is equal to the checksum at the end of the message.
     */
    static bool novatelChecksumOK(const char* rec,int len);

private:

    dsm_time_t parseBESTPOS(const char* input,double *dout,int nvars,dsm_time_t tt)
        throw();

    dsm_time_t parseBESTVEL(const char* input,double *dout,int nvars,dsm_time_t tt)
        throw();

    /**
     * Number of variables requested from Novatel BESTPOS record (sample id == 4)
     */
    int _bestPosNvars;

    /**
     * Full sample id of Novatel BESTPOS variables.
     */
    dsm_sample_id_t _bestPosId;

    /**
     * Number of variables requested from Novatel BESTVEL record (sample id == 5)
     */
    int _bestVelNvars;

    /**
     * Full sample id of Novatel BESTVEL variables.
     */
    dsm_sample_id_t _bestVelId;

    /**
     * Id of sample from Novatel BESTPOS record.  Fixed at 4.
     */
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
    static const int BESTPOS_SAMPLE_ID;
#else
    static const int BESTPOS_SAMPLE_ID = 4;
#endif

    /**
     * Id of sample from Novatel BESTPOS record.  Fixed at 5.
     */
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
    static const int BESTVEL_SAMPLE_ID;
#else
    static const int BESTVEL_SAMPLE_ID = 5;
#endif

    unsigned int _badNovatelChecksums;
};

}}	// namespace nidas namespace dynld

#endif
