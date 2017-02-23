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
    /*#ifdef notdef
    if (getSampleTags().size() > 1)
        throw InvalidParameterException(getName() +
                " can only create one sample (for all analog channels)");

    size_t nvars = stag->getVariables().size();
    #endif*/
    DSMSerialSensor::addSampleTag(stag);
}


bool
DAUSensor::
process(const Sample* samp, std::list<const Sample*>& results) throw()
{
    static LogContext logInfo(LOG_INFO);
    /*LogMessage testmsg;
    testmsg << "DAUSensor process log test";
    logInfo.log(testmsg);*/
    size_t sampLength = samp->getDataByteLength();
    if (sampLength != 50) return false; //msg must be 50 bytes long. or...???
    unsigned short header = 0x8181;//get this from xml.
    const unsigned short* dataPtr = 
        (const unsigned short*) samp->getConstVoidDataPtr();
    if(*dataPtr == header){//message is aligned
      
        // could just send to CharacterSensor parser now?
        //for now: print whole sample
        cout << "sample: ";
        for(int i=0; i < 25; i++){
            cout << hex << dataPtr[i] << " ";
        }
        cout << endl;
        //check checksum
        unsigned int checksum = 0;
        for(int i = 0; i < sampLength/2 - 1; i++){
            checksum += dataPtr[i];
        }
        checksum = checksum % 65536;
        cout << hex << "calculated checksum: " << checksum << " message checksum: " << dataPtr[24] << endl;
        return false;

    }else{//create new msg out of cached msg and current, call process again.
        int offset = 0;
        while(dataPtr[offset] != header){
            offset++;
            if(offset >= sampLength/2){
                cout << "no header found :(" << endl;
                return false; //no header found. log msg for this?
            }
        }
        //checks for time offset and data offset before creating new sample.
        if((offset != prevOffset) || 
           ((samp->getTimeTag() - prevTimeTag) > (1000000.0/30))){//30hz in microseconds
            prevTimeTag = samp->getTimeTag();
            prevId = samp->getId();
            ::memcpy(prevData, dataPtr, 50);
            prevOffset = offset;

            return false;
        }
        SampleT<char>* fullSample = getSample<char>(sampLength);
        fullSample->setTimeTag(prevTimeTag);
        fullSample->setId(prevId);
        unsigned short* newPtr = (unsigned short*) fullSample->getConstVoidDataPtr();
        ::memcpy(newPtr, &prevData[prevOffset], (25-prevOffset)*2);//length in bytes
        ::memcpy(&newPtr[25-prevOffset], dataPtr, offset*2);//length in bytes
        /*cout << "cached sample: ";
        for(int i = 0; i < 25; i++){
            cout << hex << prevData[i] << " ";
        }
        cout << endl;
        cout << "current sample: ";
        for(int i = 0; i < 25; i++){
            cout << hex << dataPtr[i] << " ";
        }
        cout << endl;*/
        bool res = DAUSensor::process(fullSample, results);
        fullSample->freeReference();//is this right
        
        prevTimeTag = samp->getTimeTag();
        prevId = samp->getId();
        ::memcpy(prevData, dataPtr, 50);
        prevOffset = offset;
        return res;
    }


#ifdef notdef//UGH!!!   
    size_t inlen = samp->getDataByteLength();
    if (inlen < 6) return false;	// bogus amount of data
    const signed char* dinptr =
        (const signed char*) samp->getConstVoidDataPtr();

    const unsigned char* ud = (const unsigned char*) dinptr;
    unsigned short checksum = (ud[1] + ud[2] + ud[3] + ud[4]) % 256;

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
