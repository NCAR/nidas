// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2017, Copyright University Corporation for Atmospheric Research
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

#include "DAUSensor.h"

#include <sstream>

using namespace nidas::dynld::isff;
using namespace std;

#include <nidas/util/Logger.h>

using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION_NS(isff,DAUSensor)

DAUSensor::DAUSensor()
{
}

DAUSensor::~DAUSensor()
{
}

void DAUSensor::addSampleTag(SampleTag* stag)
throw(InvalidParameterException)
{
#ifdef notdef
    if (getSampleTags().size() > 1)
        throw InvalidParameterException(getName() +
                " can only create one sample (for all analog channels)");

    size_t nvars = stag->getVariables().size();
#endif
    cout << "test - DAUSensor::addSampleTag" << endl;
    //test log msg as well
    static LogContext logInfo(LOG_INFO);
    LogMessage testmsg;
    testmsg << "DAUSensor addSampleTag log test";
    logInfo.log(testmsg);
    DSMSerialSensor::addSampleTag(stag);
}


bool
DAUSensor::
process(const Sample* samp, std::list<const Sample*>& results) throw()
{
#ifdef notdef
    
    size_t inlen = samp->getDataByteLength();
    if (inlen < 6) return false;	// bogus amount of data
    const signed char* dinptr =
        (const signed char*) samp->getConstVoidDataPtr();

    const unsigned char* ud = (const unsigned char*) dinptr;
    unsigned short checksum = (ud[1] + ud[2] + ud[3] + ud[4]) % 256;

    static LogContext lc(LOG_DEBUG);
    ///
    static LogContext logInfo(LOG_INFO);
    LogMessage testmsg;
    testmsg << "DAUSensor process log test";
    logInfo.log(testmsg);
    cout << "test - DAUSensor::process "<< endl;
    ///
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
#endif
    return true;
    
}


void
DAUSensor::
fromDOMElement(const xercesc::DOMElement* node)
throw(InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);
}
