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

#include "GPS_NMEA_Serial.h"
#include <nidas/core/PhysConstants.h>
#include <nidas/util/GPS.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>
#include <nidas/util/GPS.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::Logger;

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
const int GPS_NMEA_Serial::GGA_SAMPLE_ID = 1;

const int GPS_NMEA_Serial::RMC_SAMPLE_ID = 2;

const int GPS_NMEA_Serial::HDT_SAMPLE_ID = 3;
#else
const int GPS_NMEA_Serial::GGA_SAMPLE_ID;

const int GPS_NMEA_Serial::RMC_SAMPLE_ID;

const int GPS_NMEA_Serial::HDT_SAMPLE_ID;
#endif

NIDAS_CREATOR_FUNCTION(GPS_NMEA_Serial)

GPS_NMEA_Serial::GPS_NMEA_Serial():SerialSensor(),
    _ttgps(0),_ggaNvars(0),_ggaId(0),_rmcNvars(0),_rmcId(0),
    _hdtNvars(0),_hdtId(0),
    _badChecksums(0),
    _badChecksumsCount(100),
    _allowedSampleIds()
{
    _allowedSampleIds[GGA_SAMPLE_ID] = "GGA";
    _allowedSampleIds[RMC_SAMPLE_ID] = "RMC";
    _allowedSampleIds[HDT_SAMPLE_ID] = "HDT";

    // Allow the bad checksum reporting interval to be overridden.
    _badChecksumsCount =
        Logger::getScheme().getParameterT("gps_nmea_bad_checksums_count",
                                          _badChecksumsCount);
}

void GPS_NMEA_Serial::validate()
{

    SerialSensor::validate();

    const std::list<SampleTag*>& tags = getSampleTags();
    std::list<SampleTag*>::const_iterator ti = tags.begin();

    for ( ; ti != tags.end(); ++ti) {
        SampleTag* stag = *ti;

        switch(stag->getSampleId()) {
        case GGA_SAMPLE_ID:
            _ggaNvars = stag->getVariables().size();
            if (_ggaNvars != 1 && _ggaNvars != 2 && _ggaNvars != 7 && _ggaNvars != 10) {
                throw n_u::InvalidParameterException(getName(),
                        "number of variables in GGA sample","must be either 1, 2, 7, or 10");
            }
            _ggaId = stag->getId();
            break;
        case RMC_SAMPLE_ID:
            _rmcNvars = stag->getVariables().size();
            if (_rmcNvars != 1 && _rmcNvars != 2 && _rmcNvars != 6 &&_rmcNvars != 8 && _rmcNvars != 12) {
                throw n_u::InvalidParameterException(getName(),
                        "number of variables in RMC sample","must be either 1,2,6,8, or 12");
            }
            _rmcId = stag->getId();
            break;
        case HDT_SAMPLE_ID:
            _hdtNvars = stag->getVariables().size();
            if (_hdtNvars != 1) {
                throw n_u::InvalidParameterException(getName(),
                        "number of variables in HDT sample","must be 1");
            }
            _hdtId = stag->getId();
            break;
        default:
            if (_allowedSampleIds.find(stag->getSampleId()) == _allowedSampleIds.end()) {
                ostringstream ost;
                ost << "must be one of ";
                for (map<int,string>::const_iterator si = _allowedSampleIds.begin();
                        si != _allowedSampleIds.end(); ++si) {
                    int val = si->first;
                    const string& name = si->second;
                    if (si != _allowedSampleIds.begin()) ost << ", ";
                    ost << val << "(" << name << ")";
                }
                throw n_u::InvalidParameterException(getName(),
                        "sample id",ost.str());
            }
            break;
        }
    }
}

/**
 * Parse RMC NMEA record.
 *
 * If user asks for 12 variables this will parse the RMC and
 * output these variables:
 *	seconds of day
 *	receiver status
 *	latitude
 *	longitude
 *	sog
 *	cog
 *	vel ew
 *	vel ns
 *	day
 *	month
 *	year
 *	mag dev
 * If user asks for 8 variables this will parse the RMC and
 * output these variables. This is typically the case if
 * a GGA record is also being parsed making some of the above
 * variables redundant:
 *	receiver status
 *	sog
 *	cog
 *	vel ew
 *	vel ns
 *	day
 *	month
 *	year
 *
 * If user asks for 6 variables this will parse the RMC and
 * output these variables.
 *	receiver status
 *	sog
 *	cog
 *	vel ew
 *	vel ns
 *	rmclag, time difference in seconds between received sample
 *	    time tag and the time stamped in the RMC record
 * If user asks for 2 variable this will parse the RMC and
 * output these variables.
 *	receiver status
 *	rmclag, time difference in seconds between received sample
 *	    time tag and the date and time in the RMC record
 * If user asks for 1 variable this will parse the RMC and
 * output these variables.
 *	receiver status
 */
//                                        GGVEW = GGSPD * sin( GGTRK * PI/180 )
//                                        GGVNS = GGSPD * cos( GGTRK * PI/180 )
//        ggsecsday
//        |        GGSTATUS
//        |        | GGLAT          GGLON           GGSPD  GGTRK GGDAY
//        |        | |              |               |      |     | GGMONTH
//        |        | |              |               |      |     | | GGYEAR
//        |        | |              |               |      |     | | |  ggmagdev
//        |        | |              |               |      |     | | |  |
//        |\______ | |\____________ |\_____________ |\____ |\___ |\|\|\ |\_____
// $GPRMC,222504.0,A,3954.7674797,N,10507.0898443,W,000.05,214.6,160606,010.1,E*4F\r\n
//        0        1 2           3 4            5 6      7     8      9     0 1
//

dsm_time_t GPS_NMEA_Serial::parseRMC(const char* input,double *dout,int nvars,
  dsm_time_t ttraw) throw()
{
    char sep = ',';
    double lat=doubleNAN, lon=doubleNAN;
    double magvar=doubleNAN,sog=doubleNAN;
    int year, month, day, hour,minute,second;
    double f1, f2, fsec = 0.0;
    int iout = 0;
    char status = '?';
    int gpsmsod = -1;   // milliseconds of day, from HHMMSS NMEA field
    _ttgps = 0;
    int nchar;

    // input is null terminated
    for (int ifield = 0; iout < nvars; ifield++) {
        const char* cp = ::strchr(input,sep);
        if (cp == NULL) break;
        cp++;
        switch (ifield) {
        case 0:	// HHMMSS[.FF], optional output variable seconds of day
            {
                if (sscanf(input,"%2d%2d%2d%n",&hour,&minute,&second,&nchar) != 3) {
                    for (iout = 0 ; iout < nvars; iout++) dout[iout] = doubleNAN;
                    return ttraw;
                }
                int ncfsec = 0;
                if (nchar == 6 && input[6] == '.') {
                    sscanf(input+6,"%lf%n",&fsec,&ncfsec);
                }
                if (nchar != 6 || input[6 + ncfsec] != ',' ||
                        hour < 0 || hour > 23 ||
                        minute < 0 || minute > 59 ||
                        second < 0 || second > 60) {    // allow leap second=60
                    for (iout = 0 ; iout < nvars; iout++) dout[iout] = doubleNAN;
                    return ttraw;
                }
                // milliseconds of day from GPS
                gpsmsod = (hour * 3600 + minute * 60 + second) * MSECS_PER_SEC +
                    (int)rintf(fsec * MSECS_PER_SEC);
                if (nvars >= 12) dout[iout++] = gpsmsod / (double)MSECS_PER_SEC;
            }
            break;
        case 1:	// Receiver status, A=OK, V= warning, output variable stat
            status = *input;
            if (status == 'A') dout[iout++] = 1.0;
            else if (status == 'V') dout[iout++] = 0.0;
            else dout[iout++] = doubleNAN;	// var N status
            break;
        case 2:	// lat deg, lat min
            if (nvars < 12) break;
            if (sscanf(input,"%2lf%lf",&f1,&f2) == 2) lat = f1 + f2 / 60.;
            break;
        case 3:	// lat N/S, optional output variable latitude
            if (nvars < 12) break;
            if (*input == 'S') lat = -lat;
            else if (*input != 'N') lat = doubleNAN;
            dout[iout++] = lat;			// N lat
            break;
        case 4:	// lon deg, lon min
            if (nvars < 12) break;
            if (sscanf(input,"%3lf%lf",&f1,&f2) == 2) lon = f1 + f2 / 60.;
            break;
        case 5:	// lon E/W, optional output variable longitude
            if (nvars < 12) break;
            if (*input == 'W') lon = -lon;
            else if (*input != 'E') lon = doubleNAN;
            dout[iout++] = lon;			// E lon
            break;
        case 6:	// speed over ground, Knots, output variable
            if (nvars < 6) break;
            if (sscanf(input,"%lf",&f1) == 1) sog = f1 * MS_PER_KNOT;
            dout[iout++] = sog;			// spd
            break;
        case 7:	// Course made good, True, deg, output variable
            if (nvars < 6) break;
            if (sscanf(input,"%lf",&f1) == 1) {
                dout[iout++] = f1;                              // course
                dout[iout++] =  sog * sin(f1 * M_PI / 180.);	// east-west velocity
                dout[iout++] =  sog * cos(f1 * M_PI / 180.);	// north-south velocity
            }
            else {
                dout[iout++] = doubleNAN;	// var course
                dout[iout++] = doubleNAN;	// var east-west velocity
                dout[iout++] = doubleNAN;	// var north-south velocity
            }
            break;
        case 8:	// date DDMMYY
            if (sscanf(input,"%2d%2d%2d%n",&day,&month,&year,&nchar) != 3) {
                for (iout = 0 ; iout < nvars; iout++) dout[iout] = doubleNAN;
                return ttraw;
            }
            if (nchar != 6 || input[6] != ',' ||
                    day < 1 || day > 31 ||
                    month < 1 || month > 12 ||
                    year < 0 || year > 99) {
                for (iout = 0 ; iout < nvars; iout++) dout[iout] = doubleNAN;
                return ttraw;
            }

            if (status == 'A' && gpsmsod >= 0)
                _ttgps = n_u::UTime(true,year,month,day,0,0,0).toUsecs() +
                    (long long)gpsmsod * USECS_PER_MSEC;
            // output if requested
            if (nvars >= 8) {
                dout[iout++] = (double)day;
                dout[iout++] = (double)month;
                dout[iout++] = (double)year;
            }
            // If user wants GPS reporting lag, ttraw - _ttgps
            else if (nvars < 8 && nvars > 1) {
                if (_ttgps != 0)
                    dout[iout++] = (ttraw - _ttgps) / (double)USECS_PER_SEC;
                else dout[iout++] = doubleNAN;
            }
            break;
        case 9:	// Magnetic variation
            sep = '*';		// next separator is '*' before checksum
            if (nvars < 12) break;
            if (sscanf(input,"%lf",&f1) == 1) magvar = f1;
            break;
        case 10:// Mag var, E/W, optional output variable
            if (nvars < 12) break;
            if (*input == 'W') magvar = -magvar;
            else if (*input != 'E') magvar = doubleNAN;
            dout[iout++] = magvar;			// magnetic E variation
            break;
        default:
            break;
        }
        input = cp;
    }
    for ( ; iout < nvars; iout++) dout[iout] = doubleNAN;
    assert(iout == nvars);

    if (_ttgps == 0)
        return ttraw;
    else
        return _ttgps;
}

/**
 * Parse GGA NMEA message.
 *
 * If user asks for 10 variables this will parse the GGA and
 * output these variables:
 *	seconds of day
 *	latitude
 *	longitude
 *	qual
 *	nsat
 *	hordil
 *	alt
 *	geoidht
 *	dage (seconds since last DGPS update)
 *	did (DGPS station number)
 * If user asks for 7 variables this will parse the GGA and
 * output these variables.
 *	latitude
 *	longitude
 *	qual
 *	nsat
 *	hordil
 *	alt
 *	geoidht
 * If user asks for 2 variable this will parse the GGA and
 * output these variables.
 *	qual
 *	hordil
 * If user asks for 1 variable this will parse the GGA and
 * output these variables.
 *	nsat
 */
//        GGSECSDAY
//        |        GGLAT       GGLON        GGQUAL
//        |        |           |            | GGNSAT
//        |        |           |            | |  GGHORDIL
//        |        |           |            | |  |   GGALT    GGEOIDHT
//        |        |           |            | |  |   |        |
//        |\______ |\_________ |\__________ | |\ |\_ |\______ |\_____
// $GPGGA,222504.0,3954.7675,N,10507.0898,W,2,08,2.0,1726.7,M,-20.9,M,,*52\r\n
//        0        1         2 3          4 5 6  7   8      9 0     1   3
//

dsm_time_t GPS_NMEA_Serial::parseGGA(const char* input,double *dout,int nvars,
  dsm_time_t ttraw) throw()
{
    char sep = ',';
    int hour,minute,second;
    double fsec = 0.0;
    double lat=doubleNAN, lon=doubleNAN, alt=doubleNAN, geoid_ht = doubleNAN;
    int i1;
    double f1, f2;
    int iout = 0;
    int qual = 0;
    _ttgps = 0;
    int nchar;

    // input is null terminated
    for (int ifield = 0; iout < nvars; ifield++) {
        const char* cp = ::strchr(input,sep);
        if (cp == NULL) break;
        cp++;
        switch (ifield) {
        case 0:		// HHMMSS[.FF]
            {
                if (sscanf(input,"%2d%2d%2d%n",&hour,&minute,&second,&nchar) != 3) {
                    for (iout = 0 ; iout < nvars; iout++) dout[iout] = doubleNAN;
                    return ttraw;
                }
                int ncfsec = 0;
                if (nchar == 6 && input[6] == '.') {
                    sscanf(input+6,"%lf%n",&fsec,&ncfsec);
                }
                if (nchar != 6 || input[6 + ncfsec] != ',' ||
                        hour < 0 || hour > 23 ||
                        minute < 0 || minute > 59 ||
                        second < 0 || second > 60) {    // allow leap second=60
                    for (iout = 0 ; iout < nvars; iout++) dout[iout] = doubleNAN;
                    return ttraw;
                }

                // milliseconds of day from GPS
                int gpsmsod = (hour * 3600 + minute * 60 + second) * MSECS_PER_SEC +
                    (int)rintf(fsec * MSECS_PER_SEC);

                // absolute time at 00:00:00 of day from the data system time tag
                // GGA doesn't have YYMMDD field like the RMC, so we use
                // the system time tag to guess at that.
                dsm_time_t t0day = ttraw - (ttraw % USECS_PER_DAY);

                // milliseconds of day from raw timetag, rounded to nearest msec.
                int ttmsod = (ttraw - t0day + USECS_PER_MSEC/2) / USECS_PER_MSEC;

                // midnight rollovers
                if (ttmsod - gpsmsod > MSECS_PER_DAY/2) t0day += USECS_PER_DAY;
                else if (ttmsod - gpsmsod < -MSECS_PER_DAY/2) t0day -= USECS_PER_DAY;

                // time tag corrected from the HHMMSS.FF field in the GGA record
                _ttgps = t0day + (gpsmsod * (long long)USECS_PER_MSEC);

                if (nvars > 7) dout[iout++] = gpsmsod / (double)MSECS_PER_SEC;
            }
            break;
        case 1:		// latitude
            if (nvars < 7) break;
            if (sscanf(input,"%2lf%lf",&f1,&f2) != 2) break;
            lat = f1 + f2 / 60.;
            break;
        case 2:		// lat N or S
            if (nvars < 7) break;
            if (*input == 'S') lat = -lat;
            else if (*input != 'N') lat = doubleNAN;
            dout[iout++] = lat;				// var 1, lat
            break;
        case 3:		// longitude
            if (nvars < 7) break;
            if (sscanf(input,"%3lf%lf",&f1,&f2) != 2) break;
            lon = f1 + f2 / 60.;
            break;
        case 4:		// lon E or W
            if (nvars < 7) break;
            if (*input == 'W') lon = -lon;
            else if (*input != 'E') lon = doubleNAN;
            dout[iout++] = lon;				// var 2, lon
            break;
        case 5:		// fix quality
            if (sscanf(input,"%d",&qual) != 1) qual = -1;
            if (nvars < 2) break;
            if (qual >= 0) dout[iout++] = (double)qual;	// var 3, qual
            else dout[iout++] = doubleNAN;
            break;
        case 6:		// number of satelites
            if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (double)i1;
            else dout[iout++] = doubleNAN;		 // var 4, nsat
            if (nvars == 2) iout--;
            break;
        case 7:		// horizontal dilution
            if (nvars < 2) break;
            if (sscanf(input,"%lf",&f1) == 1) dout[iout++] = f1;
            else dout[iout++] = doubleNAN;		 // var 5, hor_dil
            break;
        case 8:		// altitude in meters
            if (nvars < 7) break;
            if (sscanf(input,"%lf",&f1) == 1) alt = f1;
            break;
        case 9:         // altitude units
            if (nvars < 7) break;
            if (*input != 'M') alt = doubleNAN;
            dout[iout++] = alt;				// var 6, alt
            break;
        case 10:	// height of geoid above WGS84
            if (nvars < 7) break;
            if (sscanf(input,"%lf",&f1) == 1) geoid_ht = f1;
            break;
        case 11:			// height units
            if (*input != 'M') geoid_ht = doubleNAN;
            dout[iout++] = geoid_ht;			// var 7, geoid_ht
            break;
        case 12:	// secs since DGPS update
            if (nvars < 10) break;
            if (sscanf(input,"%lf",&f1) == 1) dout[iout++] = f1;
            else dout[iout++] = doubleNAN;		// var 8, dsecs
            sep = '*';	// next separator is '*' before checksum
            break;
        case 13:	// DGPS station id
            if (nvars < 10) break;
            if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (double)i1;
            else dout[iout++] = doubleNAN;		// var 9, refid
            break;
        default:
            break;
        }
        input = cp;
    }
    for ( ; iout < nvars; iout++) dout[iout] = doubleNAN;
    assert(iout == nvars);

    // if qual is 0 or not found, don't set output timetag from NMEA,
    // leave it as the raw, received time tag.
    if (qual < 1 || _ttgps == 0)
        return ttraw;
    else
        return _ttgps;
}

/**
 * Parse HDT NMEA message.
 *
 * True Heading in degrees.
 */
//        GGHDT
//        |
//        |\_____
// $GPHDT,230.072,T*31
//        0         1
//

dsm_time_t GPS_NMEA_Serial::parseHDT(const char* input,double *dout,int,
  dsm_time_t ttraw) throw()
{
    double val;
    if (sscanf(input,"%lf",&val) == 1) dout[0] = val;
    else                               dout[0] = doubleNAN;

    // HDT NMEA message does not contain a timestamp; use the latest one
    // gathered by either parseGGA or parseRMC.
    if (_ttgps == 0)
        return ttraw;
    else
        return _ttgps;
}

bool
GPS_NMEA_Serial::
findChecksum(char& checksum, const char* rec, int len)
{
    if (len <= 0) return false;

    const char* eor = rec + len - 1;
    if (*rec == '$') rec++;

    if (*eor == '\0') eor--;    // null termination

    for ( ; eor >= rec && ::isspace(*eor); eor--);  // NL, CR

    // eor should now point to second digit of checksum
    // eor-2 should point to '*'
    if (eor < rec + 2 || *(eor - 2) != '*') return false;

    eor--;  // first digit of checksum
    char* cp;
    char cksum = ::strtol(eor,&cp,16);
    if (cp != eor + 2) return false;    // invalid checksum field length

    checksum = cksum;
    return true;
}


char
GPS_NMEA_Serial::
calcChecksum(const char* rec, int len)
{
    // We don't assume null-terminated here, so make sure we use length as
    // the limit.
    const char* eor = rec + len;

    if (rec < eor && *rec == '$')
        rec++;

    char calcsum = 0;
    for ( ; rec < eor && *rec != '*' && *rec; )
        calcsum ^= *rec++;

    return calcsum;
}


void
GPS_NMEA_Serial::
appendChecksum(char* rec, int len, int maxlen)
{
    char cksum = calcChecksum(rec, len);
    if (len + 4 < maxlen)
        sprintf(rec+len, "*%2X", cksum);
}

bool GPS_NMEA_Serial::checksumOK(const char* rec,int len)
{
    return n_u::NMEAchecksumOK(rec, len);
}

bool GPS_NMEA_Serial::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
    dsm_time_t ttfixed;
    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();

    const char* input = (const char*) samp->getConstVoidDataPtr();

    // cerr << "input=" << string(input,input+20) << " slen=" << slen << endl;
    if (slen < 7) return false;

    if (!n_u::NMEAchecksumOK(input, slen))
    {
        if (!(_badChecksums++ % _badChecksumsCount))
        {
            WLOG(("")
                 << getName() << ": bad NMEA checksum at "
                 << n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f")
                 << ", #bad=" << _badChecksums);
        }
        return false;
    }

    // Ignore 'Talker IDs' (see http://gpsd.berlios.de/NMEA.txt for details)
    input += 3;

    if (!strncmp(input,"GGA,",4) && _ggaId != 0) {
        input += 4;
        SampleT<double>* outs = getSample<double>(_ggaNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_ggaId);
        ttfixed = parseGGA(input,outs->getDataPtr(),_ggaNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed - getLagUsecs());
        results.push_back(outs);
        return true;
    }
    else if (!strncmp(input,"RMC,",4) && _rmcId != 0) {
        input += 4;
        SampleT<double>* outs = getSample<double>(_rmcNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_rmcId);
        ttfixed = parseRMC(input,outs->getDataPtr(),_rmcNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed - getLagUsecs());
        results.push_back(outs);
        return true;
    }
    else if (!strncmp(input,"HDT,",4) && _hdtId != 0) {	// True Heading, need an ALIGN capable receiver.
        input += 4;
        SampleT<double>* outs = getSample<double>(_hdtNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_hdtId);
        ttfixed = parseHDT(input,outs->getDataPtr(),_hdtNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed - getLagUsecs());
        results.push_back(outs);
        return true;
    }
    return false;
}

SampleScanner* GPS_NMEA_Serial::buildSampleScanner()
{
    MessageStreamScanner* scanr = new MessageStreamScanner();
    scanr->setNullTerminate(doesAsciiSscanfs());
    scanr->setMessageParameters(getMessageLength(),
            getMessageSeparator(),getMessageSeparatorAtEOM());
    DLOG(("%s: usec/byte=%d",getName().c_str(),getUsecsPerByte()));
    scanr->setUsecsPerByte(getUsecsPerByte());
    return scanr;
}
