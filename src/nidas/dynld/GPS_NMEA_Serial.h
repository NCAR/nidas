// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDIS_DYNLD_GPS_NMEA_SERIAL_H
#define NIDIS_DYNLD_GPS_NMEA_SERIAL_H

#include <nidas/core/SerialSensor.h>

namespace nidas { namespace dynld {

    using namespace nidas::core;

/**
 * A class for reading NMEA records from a GPS.  The process() method parses
 * GGA, RMC, and HDT NMEA messages and generates double precision floating
 * point samples.
 */
class GPS_NMEA_Serial: public SerialSensor
{
public:

    GPS_NMEA_Serial();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

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
     * Calculate the checksum of the NMEA message and return a logical
     * indicating whether it is equal to the checksum at the end of the message.
     */
    static bool
    checksumOK(const char* rec,int len);

    /**
     * Given a NMEA message in @p rec of length @p len, look for the
     * checksum and convert it to a byte value.  If found, set @p checksum
     * and return true.  Otherwise return false without changing @p
     * checksum.
     **/
    static bool
    findChecksum(char& checksum, const char* rec, int len);

    /**
     * Given a NMEA message in @p rec of length @p len, calculate the
     * checksum of the message and return it.  This only calculates the
     * checksum up to '*' or the end of the message, in case the message
     * already contains a checksum.
     **/
    static char
    calcChecksum(const char* rec, int len);

    /**
     * Calculate the checksum for the given message and append it in the
     * form "*%2X", but only if the length would not exceed maxlen,
     * including the null terminator.
     **/
    static void
    appendChecksum(char* rec, int len, int maxlen);


    dsm_time_t parseGGA(const char* input,double *dout,int nvars,dsm_time_t tt)
        throw();

    dsm_time_t parseRMC(const char* input,double *dout,int nvars,dsm_time_t tt)
        throw();

    dsm_time_t parseHDT(const char* input,double *dout,int nvars,dsm_time_t tt)
        throw();

protected:

    /**
     * Timetag set by parseGGA and parseRMC, used by parseHDT.
     */
    dsm_time_t _ttgps;

    /**
     * Number of variables requested from GGA record (sample id == 1)
     */
    int _ggaNvars;

    /**
     * Full sample id of GGA variables.
     */
    dsm_sample_id_t _ggaId;

    /**
     * Number of variables requested from RMC record (sample id == 2)
     */
    int _rmcNvars;

    /**
     * Full sample id of RMC variables.
     */
    dsm_sample_id_t _rmcId;

    /**
     * Number of variables requested from HDT record (sample id == 3)
     */
    int _hdtNvars;

    /**
     * Full sample id of HDT variables.
     */
    dsm_sample_id_t _hdtId;

    /**
     * Id of sample from GGA NMEA record.  Fixed at 1.
     */
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
    static const int GGA_SAMPLE_ID;
#else
    static const int GGA_SAMPLE_ID = 1;
#endif

    /**
     * Id of sample from RMC NMEA record.  Fixed at 2.
     */
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
    static const int RMC_SAMPLE_ID;
#else
    static const int RMC_SAMPLE_ID = 2;
#endif

    /**
     * Id of sample from HDT NMEA record.  Fixed at 3.
     */
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
    static const int HDT_SAMPLE_ID;
#else
    static const int HDT_SAMPLE_ID = 3;
#endif

    unsigned int _badChecksums;
    unsigned int _badChecksumsCount;

    /**
     * Derived classes should add their supported sample ids to the map,
     * along with a short descriptive name, so that addSampleTag() does
     * not throw an exception on an unrecognized id.
     */
    std::map<int,std::string> _allowedSampleIds;
};

}}	// namespace nidas namespace dynld

#endif
