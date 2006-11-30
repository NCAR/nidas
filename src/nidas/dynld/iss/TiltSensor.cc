/* -*- mode: C++; c-basic-offset: 4; -*-

    Copyright 2006 UCAR, NCAR, All Rights Reserved
*/

#include "TiltSensor.h"

#include <sstream>

using namespace nidas::dynld::iss;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(iss,TiltSensor)

TiltSensor::
TiltSensor() :
  checksumFailures(0)
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

**/

bool
TiltSensor::
process(const Sample* samp, std::list<const Sample*>& results) throw()
{
    size_t inlen = samp->getDataByteLength();
    if (inlen < 6) return false;	// not enough data

    const char* dinptr = (const char*) samp->getConstVoidDataPtr();
    short checksum = (dinptr[1] + dinptr[2] + dinptr[3] + dinptr[4]) % 256;

    // Compute pitch and roll.
    float pitch = (((int)dinptr[1])*256 + dinptr[2])*90.0/32768.0;
    float roll = (((int)dinptr[3])*256 + dinptr[4])*90.0/32768.0;

#ifdef DEBUG
    cerr << "inlen=" << inlen << ' ' 
	 << hex << (short)dinptr[0] 
	 << ',' << (short)dinptr[1]
	 << ',' << (short)dinptr[2]
	 << ',' << (short)dinptr[3]
	 << ',' << (short)dinptr[4]
	 << ',' << (short)dinptr[5]
	 << ", csum=" << checksum
	 << dec << ", pitch=" << pitch << ", roll=" << roll 
	 << endl;
#endif

    // Check for the header byte.
    if (dinptr[0] != '\xff') return false;

    // Now verify the checksum.
    if (checksum != dinptr[5])
    {
	++checksumFailures;
	return false;
    }

    SampleT<float>* outsamp = getSample<float>(2);
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
