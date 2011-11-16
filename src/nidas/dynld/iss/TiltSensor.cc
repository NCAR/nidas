// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include "TiltSensor.h"

#include <sstream>

using namespace nidas::dynld::iss;
using namespace std;

#include <nidas/util/Logger.h>

using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION_NS(iss,TiltSensor)

TiltSensor::TiltSensor() :
    checksumFailures(0),
    sampleId(0)
{
}

TiltSensor::~TiltSensor()
{
}

void TiltSensor::addSampleTag(SampleTag* stag)
throw(InvalidParameterException)
{
    if (getSampleTags().size() > 1)
        throw InvalidParameterException(getName() +
                " can only create one sample (pitch and roll)");

    size_t nvars = stag->getVariables().size();
    switch(nvars) 
    {
    case 2:
        sampleId = stag->getId();
        break;
    default:
        throw InvalidParameterException
            (getName() + 
             " unsupported number of variables. Must be: pitch,roll");
    }

    DSMSerialSensor::addSampleTag(stag);
}


/**
 * The data packet will consist of

 a header byte (255);
 two bytes of pitch angle information, MSB first;
 two bytes of roll angle information, MSB first;
 and a checksum.

 The checksum is calculated as:
 sum the bytes between the header and checksum byte (four bytes total);
 divide by 256;
 the remainder is the checksum.

 The angle information is represented as
 a 2's complement signed 16 bit integers in the range +32,768 to -32,768.

 If you refer to the data sheet for CXTILT02, you will notice that 32,768
 represents 90 deg and hence the correct equation for conversion would be,

 Pitch or Roll = (msb x 256 + lsb)*90/2^15

 ================================================================
 Case Reference #00002361
 ---------------------------------------------------------------
Type: Inertial
Sub Type: Tilt Sensors
Case Reason: Installation and Operation
Date Created: 11/30/2006
Last Updated: 12/4/2006

Subject:
---------------------------------------------------------------
+/- sign in AccelView are opposite what the serial message seems to suggest
from CXTILT


Question:
---------------------------------------------------------------
If I run the AccelView software with our CXTILT02 sensor, it reports in the
Tilt window the pitch is about -1.22 and roll about 0.55.  However, if I
look at the serial data, it looks like the signs should be the opposite:

ff 01 bb ff 2e e9

In this message, 01 bb is the pitch, and that number is positive right?
Likewise the roll is ff 2e, which is negative.  Is there an explanation for
this discrepancy?  Thanks.


Response:
---------------------------------------------------------------
We have verified this behavior in house and it appears as though the data
sheet is incorrect.  You can simply multiply the end result values by -1 to
obtain the correct values.  Accel-View does appear to be providing correct
roll and pitch values in conjunction with the orientation of theunit.  We
apologize for the error and will be making a correction shortly.

Sincerely,

Mike Smith
Crossbow Technology, Inc
www.xbow.com

 **/

inline float
decode_angle(const signed char* dp)
{
    // this should sign extend
    int s = dp[0];
    s <<= 8;
    s |= (((int)dp[1]) & 0xff);
    return -(float)s * 90.0 / 32768.0;
}

bool
TiltSensor::
process(const Sample* samp, std::list<const Sample*>& results) throw()
{
    size_t inlen = samp->getDataByteLength();
    if (inlen < 6) return false;	// bogus amount of data
    const signed char* dinptr =
        (const signed char*) samp->getConstVoidDataPtr();
    const unsigned char* ud = (const unsigned char*) dinptr;
    unsigned short checksum = (ud[1] + ud[2] + ud[3] + ud[4]) % 256;

    // Compute pitch and roll.
    float pitch = decode_angle(dinptr+1);
    float roll = decode_angle(dinptr+3);

    static LogContext lc(LOG_DEBUG);
    if (lc.active())
    {
        LogMessage msg;
        msg << "inlen=" << inlen << ' ' ;
        msg.format ("%02x,%02x,%02x,%02x,%02x,%02x, csum=%02x",
                ud[0], ud[1], ud[2], ud[3], ud[4], ud[5], checksum);
        msg << "; pitch=" << pitch << ", roll=" << roll;
        lc.log (msg);
    }

    // Check for the header byte.
    if (ud[0] != 0xff) 
    {
        PLOG(("unexpected header byte, skipping bad sample"));
        return false;
    }

    // Now verify the checksum.
    if (checksum != ud[5])
    {
        if (checksumFailures++ % 60 == 0)
        {
            PLOG(("Checksum failures: ") << checksumFailures);
        }
        return false;
    }

    SampleT<float>* outsamp = getSample<float>(2);
    outsamp->setTimeTag(samp->getTimeTag());
    outsamp->setId(sampleId);

    float* values = outsamp->getDataPtr();
    values[0] = pitch;
    values[1] = roll;
    results.push_back(outsamp);
    return true;
}


void
TiltSensor::
fromDOMElement(const xercesc::DOMElement* node)
throw(InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);
}
