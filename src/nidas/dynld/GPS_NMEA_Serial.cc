// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include <nidas/dynld/GPS_NMEA_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

const int GPS_NMEA_Serial::GGA_SAMPLE_ID = 1;
const int GPS_NMEA_Serial::RMC_SAMPLE_ID = 2;

NIDAS_CREATOR_FUNCTION(GPS_NMEA_Serial)

GPS_NMEA_Serial::GPS_NMEA_Serial():DSMSerialSensor(),
    _ggaNvars(0),_ggaId(0),_rmcNvars(0),_rmcId(0)
{

}

GPS_NMEA_Serial::~GPS_NMEA_Serial()
{
}

void GPS_NMEA_Serial::addSampleTag(SampleTag* stag)
throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);

    switch(stag->getSampleId()) {
    case GGA_SAMPLE_ID:
        _ggaNvars = stag->getVariables().size();
        if (_ggaNvars != 1 && _ggaNvars != 7 && _ggaNvars != 10) {
            throw n_u::InvalidParameterException(getName(),
                    "number of variables in GGA sample","must be either 1, 7, or 10");
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
    default:
        {
            ostringstream ost;
            ost << "must be either " <<
                GGA_SAMPLE_ID << "(GGA) or "  <<
                RMC_SAMPLE_ID << "(RMC)";
            throw n_u::InvalidParameterException(getName(),
                    "sample id",ost.str());
        }
        break;
    }
}

/**
 * Parse RMC NMEA record.
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
 *	rmclag, time difference in seconds between received sample
 *	    time tag and the time stamped in the RMC record
 *	receiver status
 *	sog
 *	cog
 *	vel ew
 *	vel ns
 * If user asks for 2 variable this will parse the RMC and
 * output these variables.
 *	rmclag, time difference in seconds between received sample
 *	    time tag and the date and time in the RMC record
 *	receiver status
 * If user asks for 1 variable this will parse the RMC and
 * output these variables.
 *	receiver status
 */
//                                        GGVEW = GGSPD * sin( GGTRK * PI/180 )
//                                        GGVNS = GGSPD * cos( GGTRK * PI/180 )
//        ggsecsday
//        |        GGSTATUS
//        |        | gglat        gglon         GGSPD  GGTRK GGDAY
//        |        | |            |             |      |     | GGMONTH
//        |        | |            |             |      |     | | GGYEAR
//        |        | |            |             |      |     | | |  ggmagdev
//        |        | |            |             |      |     | | |  |
//        |\______ | |\__________ |\___________ |\____ |\___ |\|\|\ |\_____
// $GPRMC,222504.0,A,3954.78106,N,10507.09950,W,000.05,214.6,160606,010.1,E*4F\r\n
//        0        1 2          3 4           5 6      7     8      9     0 1
//

dsm_time_t GPS_NMEA_Serial::parseRMC(const char* input,float *dout,int nvars,
        dsm_time_t tt) throw()
{
    char sep = ',';
    float lat=floatNAN, lon=floatNAN;
    float magvar=floatNAN,sog=floatNAN;
    int year, month, day, hour,minute;
    float f1, f2, second;
    int iout = 0;
    char status = '?';
    dsm_time_t ttgps = 0;
    int gpsmsod = -1;   // milliseconds of day, from HHMMSS NMEA field

    // input is null terminated
    for (int ifield = 0; iout < nvars; ifield++) {
        const char* cp = ::strchr(input,sep);
        if (cp == NULL) break;
        cp++;
        switch (ifield) {
        case 0:	// HHMMSS, optional output variable seconds of day
            if (sscanf(input,"%2d%2d%f",&hour,&minute,&second) == 3) {
                // milliseconds of day from GPS
                gpsmsod = (hour * 3600 + minute * 60) * MSECS_PER_SEC +
                    rintf(second * MSECS_PER_SEC);
                if (nvars >= 12) dout[iout++] = gpsmsod / (float)MSECS_PER_SEC;
            }
            else {
                gpsmsod = -1;
                if (nvars >= 12) dout[iout++] = floatNAN;
            }
            break;
        case 1:	// Receiver status, A=OK, V= warning, output variable stat
            status = *input;
            if (status == 'A') dout[iout++] = 1.0;
            else if (*input == 'V') {
                // if this is the second output variable then the first is
                // the time difference. Set to NAN if no GPS lock.
                if (iout == 1) dout[0] = floatNAN;
                dout[iout++] = 0.0;
            }
            else dout[iout++] = floatNAN;	// var N status
            break;
        case 2:	// lat deg, lat min
            if (nvars < 12) break;
            if (sscanf(input,"%2f%f",&f1,&f2) == 2) lat = f1 + f2 / 60.;
            break;
        case 3:	// lat N/S, optional output variable latitude
            if (nvars < 12) break;
            if (*input == 'S') lat = -lat;
            else if (*input != 'N') lat = floatNAN;
            dout[iout++] = lat;			// var N lat
            break;
        case 4:	// lon deg, lon min
            if (nvars < 12) break;
            if (sscanf(input,"%3f%f",&f1,&f2) == 2) lon = f1 + f2 / 60.;
            break;
        case 5:	// lon E/W, optional output variable longitude
            if (nvars < 12) break;
            if (*input == 'W') lon = -lon;
            else if (*input != 'E') lon = floatNAN;
            dout[iout++] = lon;			// var N, lon
            break;
        case 6:	// speed over ground, Knots, output variable 
            if (nvars < 6) break;
            if (sscanf(input,"%f",&f1) == 1) sog = f1 * MS_PER_KNOT;
            dout[iout++] = sog;			// var ?, spd
            break;
        case 7:	// Course made good, True, deg, output variable 
            if (nvars < 6) break;
            if (sscanf(input,"%f",&f1) == 1) {
                dout[iout++] = f1;
                dout[iout++] =  sog * sin(f1 * M_PI / 180.);
                dout[iout++] =  sog * cos(f1 * M_PI / 180.);
            }
            else {
                dout[iout++] = floatNAN;	// var course
                dout[iout++] = floatNAN;	// var east-west velocity
                dout[iout++] = floatNAN;	// var north-south velocity
            }
            break;
        case 8:	// date DDMMYY
            if (sscanf(input,"%2d%2d%2d",&day,&month,&year) == 3) {
                if (status == 'A' && gpsmsod >= 0)
                    ttgps = n_u::UTime(true,year,month,day,0,0,0).toUsecs() +
                        (long long)gpsmsod * USECS_PER_MSEC;
                // output if requested
                if (nvars >= 8) {
                    dout[iout++] = (float)day;
                    dout[iout++] = (float)month;
                    dout[iout++] = (float)year;
                }
                // If user wants GPS reporting lag, tt - ttgps
                else if (nvars < 8 && nvars > 1) {
                    if (ttgps != 0)
                        dout[iout++] = (tt - ttgps) / (float)USECS_PER_SEC;
                    else dout[iout++] = floatNAN;
                }
            }
            else {
                if (nvars >= 8) {
                    dout[iout++] = floatNAN;	// day
                    dout[iout++] = floatNAN;	// month
                    dout[iout++] = floatNAN;	// year
                }
                else if (nvars < 8 && nvars > 1) dout[iout++] = floatNAN;   // GPS lag
            }
            break;
        case 9:	// Magnetic variation
            sep = '*';		// next separator is '*' before checksum
            if (nvars < 12) break;
            if (sscanf(input,"%f",&f1) == 1) magvar = f1;
            break;
        case 10:// Mag var, E/W, optional output variable
            if (nvars < 12) break;
            if (*input == 'W') magvar = -magvar;
            else if (*input != 'E') magvar = floatNAN;
            dout[iout++] = magvar;			// var ?, magnetic variation
            break;
        default:
            break;
        }
        input = cp;
    }
    for ( ; iout < nvars; iout++) dout[iout] = floatNAN;
    assert(iout == nvars);

    if (ttgps == 0)
        return tt;
    else
        return ttgps;
}

/**
 * Parse GGA NMEA message.
 */
//        GGSECSDAY
//        |        GGLAT        GGLON         GGQUAL
//        |        |            |             | GGNSAT
//        |        |            |             | |  GGHORDIL
//        |        |            |             | |  |   GGALT    GGEOIDHT
//        |        |            |             | |  |   |        |
//        |\______ |\__________ |\___________ | |\ |\_ |\______ |\_____
// $GPGGA,222504.0,3954.78106,N,10507.09950,W,2,08,2.0,1726.7,M,-20.9,M,,*52\r\n
//        0        1          2 3           4 5 6  7   8      9 0     1   3
//
/*
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
 * If user asks for 1 variable this will parse the GGA and
 * output these variables.
 *	nsat
 */

dsm_time_t GPS_NMEA_Serial::parseGGA(const char* input,float *dout,int nvars,
        dsm_time_t tt) throw()
{
    char sep = ',';
    int hour,minute;
    float second;
    float lat=floatNAN, lon=floatNAN, alt=floatNAN, geoid_ht = floatNAN;
    int i1;
    float f1, f2;
    int iout = 0;
    dsm_time_t ttgps = 0;

    // input is null terminated
    for (int ifield = 0; iout < nvars; ifield++) {
        const char* cp = ::strchr(input,sep);
        if (cp == NULL) break;
        cp++;
        switch (ifield) {
        case 0:		// HHMMSS
            if (sscanf(input,"%2d%2d%f",&hour,&minute,&second) == 3) {
                // milliseconds of day from GPS
                int gpsmsod = (hour * 3600 + minute * 60) * MSECS_PER_SEC +
                    rintf(second * MSECS_PER_SEC);

                // absolute time at 00:00:00 of day from the data system time tag
                // GGA doesn't have YYMMDD field like the RMC, so we use
                // the system time tag to quess at that.
                dsm_time_t t0day = tt - (tt % USECS_PER_DAY);

                // milliseconds of day from timetag, rounded to nearest msec.
                int ttmsod = (tt - t0day + USECS_PER_MSEC/2) / USECS_PER_MSEC;

                // midnight rollovers
                if (ttmsod - gpsmsod > MSECS_PER_DAY/2) t0day += USECS_PER_DAY;
                else if (ttmsod - gpsmsod < -MSECS_PER_DAY/2) t0day -= USECS_PER_DAY;

                // time tag corrected from the HHMMSS.S field in the GGA record
                ttgps = t0day + (gpsmsod * (long long)USECS_PER_MSEC);

                if (nvars > 7) dout[iout++] = gpsmsod / (float)MSECS_PER_SEC;
            }
            else {
                if (nvars > 7) dout[iout++] = floatNAN;
            }
            break;
        case 1:		// latitude
            if (nvars < 7) break;
            if (sscanf(input,"%2f%f",&f1,&f2) != 2) break;
            lat = f1 + f2 / 60.;
            break;
        case 2:		// lat N or S
            if (nvars < 7) break;
            if (*input == 'S') lat = -lat;
            else if (*input != 'N') lat = floatNAN;
            dout[iout++] = lat;				// var 1, lat
            break;
        case 3:		// longitude
            if (nvars < 7) break;
            if (sscanf(input,"%3f%f",&f1,&f2) != 2) break;
            lon = f1 + f2 / 60.;
            break;
        case 4:		// lon E or W
            if (nvars < 7) break;
            if (*input == 'W') lon = -lon;
            else if (*input != 'E') lon = floatNAN;
            dout[iout++] = lon;				// var 2, lon
            break;
        case 5:		// fix quality
            if (nvars < 7) break;
            if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (float)i1;
            else dout[iout++] = floatNAN;		// var 3, qual
            break;
        case 6:		// number of satelites
            if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (float)i1;
            else dout[iout++] = floatNAN;		 // var 4, nsat
            break;
        case 7:		// horizontal dilution
            if (nvars < 7) break;
            if (sscanf(input,"%f",&f1) == 1) dout[iout++] = f1;
            else dout[iout++] = floatNAN;		 // var 5, hor_dil
            break;
        case 8:		// altitude in meters
            if (nvars < 7) break;
            if (sscanf(input,"%f",&f1) == 1) alt = f1;
            break;
        case 9:		// altitude units
            if (nvars < 7) break;
            if (*input != 'M') alt = floatNAN;
            dout[iout++] = alt;				// var 6, alt
            break;
        case 10:	// height of geoid above WGS84
            if (nvars < 7) break;
            if (sscanf(input,"%f",&f1) == 1) geoid_ht = f1;
            break;
        case 11:			// height units
            if (*input != 'M') geoid_ht = floatNAN;
            dout[iout++] = geoid_ht;			// var 7, geoid_ht
            break;
        case 12:	// secs since DGPS update
            if (nvars < 10) break;
            if (sscanf(input,"%f",&f1) == 1) dout[iout++] = f1;
            else dout[iout++] = floatNAN; 		// var 8, dsecs
            sep = '*';	// next separator is '*' before checksum
            break;
        case 13:	// DGPS station id
            if (nvars < 10) break;
            if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (float)i1;
            else dout[iout++] = floatNAN;		// var 9, refid
            break;
        default:
            break;
        }
        input = cp;
    }
    for ( ; iout < nvars; iout++) dout[iout] = floatNAN;
    assert(iout == nvars);

    if (ttgps == 0)
        return tt;
    else
        return ttgps;
}

bool GPS_NMEA_Serial::process(const Sample* samp,list<const Sample*>& results)
throw()
{
    dsm_time_t ttfixed;
    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();
    if (slen < 7) return false;

    const char* input = (const char*) samp->getConstVoidDataPtr();

    // cerr << "input=" << string(input,input+20) << " slen=" << slen << endl;

    if (!strncmp(input,"$GPGGA,",7) && _ggaId != 0) {
        input += 7;
        SampleT<float>* outs = getSample<float>(_ggaNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_ggaId);
        ttfixed = parseGGA(input,outs->getDataPtr(),_ggaNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed);
        results.push_back(outs);
        return true;
    }
    else if (!strncmp(input,"$GPRMC,",7) && _rmcId != 0) {
        input += 7;
        SampleT<float>* outs = getSample<float>(_rmcNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_rmcId);
        ttfixed = parseRMC(input,outs->getDataPtr(),_rmcNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed);
        results.push_back(outs);
        return true;
    }
    return false;
}

