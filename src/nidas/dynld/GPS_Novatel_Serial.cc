// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include <nidas/dynld/GPS_Novatel_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
const int GPS_Novatel_Serial::BESTPOS_SAMPLE_ID = 4;

const int GPS_Novatel_Serial::BESTVEL_SAMPLE_ID = 5;
#endif

NIDAS_CREATOR_FUNCTION(GPS_Novatel_Serial)

GPS_Novatel_Serial::GPS_Novatel_Serial() : GPS_NMEA_Serial(),
    _bestPosNvars(0),_bestPosId(0),
    _bestVelNvars(0),_bestVelId(0),_badNovatelChecksums(0)
{
    _allowedSampleIds[BESTPOS_SAMPLE_ID] = "BESTPOS";
    _allowedSampleIds[BESTVEL_SAMPLE_ID] = "BESTVEL";
}

void GPS_Novatel_Serial::addSampleTag(SampleTag* stag)
  throw(n_u::InvalidParameterException)
{

    switch(stag->getSampleId()) {
    case BESTPOS_SAMPLE_ID:
        _bestPosNvars = stag->getVariables().size();
        if (_bestPosNvars != 3) {
            throw n_u::InvalidParameterException(getName(),
                    "number of variables in BESTPOS sample","must be 1");
        }
        _bestPosId = stag->getId();
        break;
    case BESTVEL_SAMPLE_ID:
        _bestVelNvars = stag->getVariables().size();
        if (_bestVelNvars != 5) {
            throw n_u::InvalidParameterException(getName(),
                    "number of variables in BESTVEL sample","must be 1");
        }
        _bestVelId = stag->getId();
        break;
    default:
        break;
    }
    GPS_NMEA_Serial::addSampleTag(stag);
}

dsm_time_t GPS_Novatel_Serial::parseBESTPOS(const char* input,double *dout,int nvars,
  dsm_time_t tt) throw()
{
    char sep = ',';
    double lat=doubleNAN, lon=doubleNAN, alt=doubleNAN;
    int iout = 0;

    // input is null terminated
    for (int ifield = 0; iout < nvars; ifield++) {
        const char* cp = ::strchr(input,sep);
        if (cp == NULL) break;
        cp++;
        switch (ifield) {
        case 10:	// lat
            if (sscanf(input,"%lf",&lat) == 1) dout[iout++] = lat;
            else dout[iout++] = doubleNAN;
            break;

        case 11:	// lon
            if (sscanf(input,"%lf",&lon) == 1) dout[iout++] = lon;
            else dout[iout++] = doubleNAN;
            break;

        case 12:		// alt
            if (sscanf(input,"%lf",&alt) == 1) dout[iout++] = alt;
            else dout[iout++] = doubleNAN;
            break;
        default:
            break;
        }
        input = cp;
    }
    for ( ; iout < nvars; iout++) dout[iout] = doubleNAN;
    assert(iout == nvars);

    // not contain a timestamp; use the latest one
    // gathered by either parseGGA or parseRMC.
    if (_ttgps == 0)
        return tt;
    else
        return _ttgps;
}

dsm_time_t GPS_Novatel_Serial::parseBESTVEL(const char* input,double *dout,int nvars,
  dsm_time_t tt) throw()
{
    char sep = ',';
    double trk=doubleNAN, spd=doubleNAN, vspd=doubleNAN, latency=0.0;
    int iout = 0;

    // input is null terminated
    for (int ifield = 0; iout < nvars; ifield++) {
        const char* cp = ::strchr(input,sep);
        if (cp == NULL) break;
        cp++;
        switch (ifield) {
        case 10:	// latency in seconds
            sscanf(input,"%lf",&latency);
            break;

        case 12:	// horizontal speed
            if (sscanf(input,"%lf",&spd) == 1) dout[iout++] = spd;
            else dout[iout++] = doubleNAN;
            break;

        case 13:	// track over ground wrt to true north.
            if (sscanf(input,"%lf",&trk) == 1) dout[iout++] = trk;
            else dout[iout++] = doubleNAN;
            dout[iout++] =  spd * sin(trk * M_PI / 180.);    // east-west velocity
            dout[iout++] =  spd * cos(trk * M_PI / 180.);    // north-south velocity
            break;

        case 14:	// vertical speed
            if (sscanf(input,"%lf",&vspd) == 1) dout[iout++] = vspd;
            else dout[iout++] = doubleNAN;
            break;

        default:
            break;
        }
        input = cp;
    }
    for ( ; iout < nvars; iout++) dout[iout] = doubleNAN;
    assert(iout == nvars);

    // not contain a timestamp; use the latest one
    // gathered by either parseGGA or parseRMC.
    if (_ttgps == 0)
        return tt;
    else
        return _ttgps - (int)(latency * USECS_PER_SEC);
}

namespace {

    // Novatel checksum algorithm, from 
    // www.novatel.com/assets/Documents/Bulletins/apn030.pdf
    const unsigned int CRC32_POLYNOMIAL = 0xEDB88320;
    unsigned int CRC32Value(int i)
    {
        int j;
        unsigned int ulCRC = i;
        for (j=8;j>0;j--)
        {
            if (ulCRC & 1)
                ulCRC = (ulCRC >> 1)^CRC32_POLYNOMIAL;
            else ulCRC >>= 1;
        }
        return ulCRC;
    }

    unsigned int CalculateBlockCRC32(unsigned int ulCount,const unsigned char *ucBuffer)
    {
        unsigned int ulTemp1;
        unsigned int ulTemp2;
        unsigned int ulCRC = 0;
        while (ulCount-- != 0)
        {
            ulTemp1 = (ulCRC >> 8) & 0x00FFFFFFL;
            ulTemp2 = CRC32Value(((int)ulCRC^*ucBuffer++)&0xff);
            ulCRC = ulTemp1^ulTemp2;
        }
        return ulCRC;
    }
}

bool GPS_Novatel_Serial::novatelChecksumOK(const char* rec,int len)
{

    if (len <= 0) return false;

    const char* eor = rec + len - 1;
    if (*rec == '#') rec++;

    if (*eor == '\0') eor--;    // null termination

    for ( ; eor >= rec && ::isspace(*eor); eor--);  // NL, CR
    // eor should now point to last digit of checksum

    eor -= 7;   // first digit of checksum
    if (eor < rec) return false;

    char* cp;
    unsigned int cksum = ::strtoul(eor,&cp,16);
    if (cp != eor + 8) return false;    // less than 8 valid hex digits

    eor--;      // should point to '*'
    if (eor < rec || *eor != '*') return false;

    unsigned int calcsum = CalculateBlockCRC32((unsigned int)(eor-rec),(const unsigned char*)rec);
    return cksum == calcsum;
}

bool GPS_Novatel_Serial::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();

    const char* input = (const char*) samp->getConstVoidDataPtr();

    // if the message starts with a '$' then assume its a NMEA message and
    // use the base class process method
    if (slen > 0 && input[0] == '$') 
        return GPS_NMEA_Serial::process(samp,results);

    if (!novatelChecksumOK(input,slen)) {
        if (!(_badNovatelChecksums++ % 100) && _badNovatelChecksums > 1) WLOG(("%s: bad Novatel checksum at ",getName().c_str()) <<
                n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") << ", #bad=" << _badNovatelChecksums);
        return false;
    }

    // cerr << "input=" << string(input,input+20) << " slen=" << slen << endl;
    if (slen < 10) return false;

    dsm_time_t ttfixed;
    if (_bestPosId != 0 && !strncmp(input, "#BESTPOSA,", 10)) {  // Novatel BESTPOS
        input += 10;
        SampleT<double>* outs = getSample<double>(_bestPosNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_bestPosId);
        ttfixed = parseBESTPOS(input,outs->getDataPtr(),_bestPosNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed);
        results.push_back(outs);
        return true;
    }
    else if (_bestVelId != 0 && !strncmp(input, "#BESTVELA,", 10)) {  // Novatel BESTVEL
        input += 10;
        SampleT<double>* outs = getSample<double>(_bestVelNvars);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(_bestVelId);
        ttfixed = parseBESTVEL(input,outs->getDataPtr(),_bestVelNvars,samp->getTimeTag());
        outs->setTimeTag(ttfixed);
        results.push_back(outs);
        return true;
    }
    return false;
}

