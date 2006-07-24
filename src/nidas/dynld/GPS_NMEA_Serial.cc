/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/GPS_NMEA_Serial.h>
#include <nidas/core/PhysConstants.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

const int GPS_NMEA_Serial::GGA_SAMPLE_ID = 1;
const int GPS_NMEA_Serial::RMC_SAMPLE_ID = 2;


NIDAS_CREATOR_FUNCTION(GPS_NMEA_Serial)

void GPS_NMEA_Serial::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);

    switch(stag->getSampleId()) {
    case GGA_SAMPLE_ID:
	ggaNvars = stag->getVariables().size();
	ggaId = stag->getId();
	break;
    case RMC_SAMPLE_ID:
	rmcNvars = stag->getVariables().size();
	if (rmcNvars != 8 && rmcNvars != 12) {
	    ostringstream ost;
	    ost << "must be either 12 or 8 ";
	    throw n_u::InvalidParameterException(getName(),
		"number of variables in RMC sample",ost.str());
	}
	rmcId = stag->getId();
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
 * a GGA record is also being parsed making some of these
 * variable redundant:
 *	receiver status
 *	sog
 *	cog
 *	vel ew
 *	vel ns
 *	day
 *	month
 *	year
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

void GPS_NMEA_Serial::parseRMC(const char* input,float *dout,int nvars) throw()
{
    char sep = ',';
    float lat=floatNAN, lon=floatNAN;
    float magvar=floatNAN,sog=floatNAN;
    int i1,i2,i3;
    float f1,f2;
    int iout = 0;
    // input is null terminated
    for (int ifield = 0; ; ifield++) {
	const char* cp = ::strchr(input,sep);
	if (cp == NULL) break;
	cp++;
	switch (ifield) {
	case 0:	// HHMMSS, optional output variable seconds of day
	    if (nvars < 12) break;
	    if (sscanf(input,"%2d%2d%2d",&i1,&i2,&i3) != 3) break;
	    dout[iout++] = i1 * 3600.0 + i2 * 60.0 + i3;
	    break;
	case 1:	// Receiver status, A=OK, V= warning, output variable stat
	    if (*input == 'A') dout[iout++] = 1.0;
	    else if (*input == 'V') dout[iout++] = 0.0;
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
	    if (sscanf(input,"%f",&f1) == 1) sog = f1 * MS_PER_KNOT;
	    dout[iout++] = sog;			// var ?, spd
	    break;
	case 7:	// Course made good, True, deg, output variable 
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
	    if (sscanf(input,"%2d%2d%2d",&i1,&i2,&i3) == 3) {
	        dout[iout++] = (float)i1;	// day
	        dout[iout++] = (float)i2;	// month
	        dout[iout++] = (float)i3;	// year
	    }
	    else {
	        dout[iout++] = floatNAN;	// day
	        dout[iout++] = floatNAN;	// month
	        dout[iout++] = floatNAN;	// year
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
void GPS_NMEA_Serial::parseGGA(const char* input,float *dout,int nvars) throw()
{
    char sep = ',';
    float lat=floatNAN, lon=floatNAN, alt=floatNAN, geoid_ht = floatNAN;
    int i1,i2,i3;
    float f1,f2;
    int iout = 0;
    // input is null terminated
    for (int ifield = 0; ; ifield++) {
	const char* cp = ::strchr(input,sep);
	if (cp == NULL) break;
	cp++;
	switch (ifield) {
	case 0:		// HHMMSS
	    if (sscanf(input,"%2d%2d%2d",&i1,&i2,&i3) != 3) break;
	    dout[iout++] = i1 * 3600.0 + i2 * 60.0 + i3;// var 0 secs of day
	    break;
	case 1:		// latitude
	    if (sscanf(input,"%2f%f",&f1,&f2) != 2) break;
	    lat = f1 + f2 / 60.;
	    break;
	case 2:		// lat N or S
	    if (*input == 'S') lat = -lat;
	    else if (*input != 'N') lat = floatNAN;
	    dout[iout++] = lat;				// var 1, lat
	    break;
	case 3:		// longitude
	    if (sscanf(input,"%3f%f",&f1,&f2) != 2) break;
	    lon = f1 + f2 / 60.;
	    break;
	case 4:		// lon E or W
	    if (*input == 'W') lon = -lon;
	    else if (*input != 'E') lon = floatNAN;
	    dout[iout++] = lon;				// var 2, lon
	    break;
	case 5:		// fix quality
	    if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (float)i1;
	    else dout[iout++] = floatNAN;		// var 3, qual
	    break;
	case 6:		// number of satelites
	    if (sscanf(input,"%d",&i1) == 1) dout[iout++] = (float)i1;
	    else dout[iout++] = floatNAN;		 // var 4, nsat
	    break;
	case 7:		// horizontal dilution
	    if (sscanf(input,"%f",&f1) == 1) dout[iout++] = f1;
	    else dout[iout++] = floatNAN;		 // var 5, hor_dil
	    break;
	case 8:		// altitude in meters
	    if (sscanf(input,"%f",&f1) == 1) alt = f1;
	    break;
	case 9:		// altitude units
	    if (*input != 'M') alt = floatNAN;
	    dout[iout++] = alt;				// var 6, alt
	    break;
	case 10:	// height of geoid above WGS84
	    if (sscanf(input,"%f",&f1) == 1) geoid_ht = f1;
	    break;
	case 11:			// height units
	    if (*input != 'M') geoid_ht = floatNAN;
	    dout[iout++] = geoid_ht;			// var 7, geoid_ht
	    break;
	case 12:	// secs since DGPS update
	    if (sscanf(input,"%f",&f1) == 1) dout[iout++] = f1;
	    else dout[iout++] = floatNAN; 		// var 8, dsecs
	    sep = '*';	// next separator is '*' before checksum
	    break;
	case 13:	// DGPS station id
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
}

bool GPS_NMEA_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();
    if (slen < 7) return false;

    const char* input = (const char*) samp->getConstVoidDataPtr();

    // cerr << "input=" << string(input,input+20) << " slen=" << slen << endl;

    if (!strncmp(input,"$GPGGA,",7)) {
	input += 7;
	SampleT<float>* outs = getSample<float>(ggaNvars);
	outs->setTimeTag(samp->getTimeTag());
	outs->setId(ggaId);
	parseGGA(input,outs->getDataPtr(),ggaNvars);
	results.push_back(outs);
	return true;
    }
    else if (!strncmp(input,"$GPRMC,",7)) {
	input += 7;
	SampleT<float>* outs = getSample<float>(rmcNvars);
	outs->setTimeTag(samp->getTimeTag());
	outs->setId(rmcId);
	parseRMC(input,outs->getDataPtr(),rmcNvars);
	results.push_back(outs);
	return true;
    }
    return false;
}

