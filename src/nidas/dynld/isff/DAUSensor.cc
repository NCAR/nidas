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
#include <nidas/core/Variable.h>

#include <sstream>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

#include <nidas/util/Logger.h>

using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION_NS(isff,DAUSensor)

DAUSensor::DAUSensor():
    _cvtr(0),
    _prevTimeTag(0),
    _prevData(),
    _prevOffset(0)
{
}

DAUSensor::~DAUSensor()
{
}

void DAUSensor::init() throw(InvalidParameterException)
{
    DSMSerialSensor::init();
    _cvtr = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_BIG_ENDIAN);
}
void DAUSensor::addSampleTag(SampleTag* stag)
throw(InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);
}


bool
DAUSensor::
process(const Sample* samp, std::list<const Sample*>& results) throw()
{
    size_t sampLength = samp->getDataByteLength();
    if (sampLength != 50){
        PLOG(("Message length incorrect."));
        return false; //msg must be 50 bytes long
    }

    //get message separator from xml and cast to short from string
    string sep = getMessageSeparator();
    unsigned short header = (((unsigned short) sep[0]) << 8) | 
        (((unsigned short) sep[1]) & 0x00ff);
    unsigned char* sampPtr = (unsigned char*) samp->getConstVoidDataPtr();
    
    int offset = -1;
    //combine 2 chars to get short--header may not be on even byte boundary
    for(size_t i = 0; i < sampLength-1; i++){
        unsigned short test = (sampPtr[i] << 8) | sampPtr[i+1];
        if(test == header){
            offset = i;
            break;
        }
    }
    if(offset == -1){
        PLOG(("Message header not found."));
        return false;
    }else if(offset != 0){//reconstruct message from prev and current samples
         //checks for bad time or data offset, saves current sample before exit
        if((offset != _prevOffset) || 
           ((samp->getTimeTag() - _prevTimeTag) > (1000000/30))){//30hz limit
            _prevTimeTag = samp->getTimeTag();
            _prevData.assign(&sampPtr[offset], &sampPtr[sampLength]);
            _prevOffset = offset;
            PLOG(("Message not continuous across adjacent samples."));
            return false;
        }
        //add rest of message to previously cached message start
        _prevData.insert(_prevData.end(), &sampPtr[0], &sampPtr[offset]);

    }else{//msg is aligned.
        _prevData.assign(&sampPtr[0], &sampPtr[sampLength]);
    }

    unsigned short* dataPtr = (unsigned short*) &_prevData[0];
    
    //message is big-endian, convert to little-endian to match system.
    for(int i = 0; i < 25; i++){
        dataPtr[i] = _cvtr->uint16Value(dataPtr[i]);
    }

    //check checksum
    unsigned int checksum = 0;
    for(unsigned int i = 0; i < sampLength/2 - 1; i++){
        checksum += dataPtr[i];
    }
    checksum = checksum & 0xffff;
    if(checksum != dataPtr[24]){
        PLOG(("Checksum failure."));
        return false;
    }

    list<SampleTag*>& tags= getSampleTags();
    SampleTag* stag = tags.front();//assuming only one sampletag present.
    const vector<Variable*>& vars = stag->getVariables();

    //create processed sample
    SampleT<float>* outsamp = getSample<float>(vars.size()); 
    outsamp->setTimeTag(samp->getTimeTag());
    outsamp->setId(stag->getId());
    float * outPtr = (float*) outsamp->getDataPtr();

    //copy correct channels to sample, convert to float, and apply calfile.
    for(size_t i = 0; i < vars.size(); i++){
        int channel = vars[i]->getParameter("channel")->getNumericValue(0);
        *outPtr = (float) dataPtr[channel];
        VariableConverter* conv = vars[i]->getConverter();
        *outPtr = conv->convert(outsamp->getTimeTag(), *outPtr);
        outPtr++;
    }

    results.push_back(outsamp);
    
    //cache part of current sample that has not yet been used.
    if(offset != 0){
        _prevTimeTag = samp->getTimeTag();
        _prevOffset = offset;
        _prevData.assign(&sampPtr[offset], &sampPtr[sampLength]);
    }
    return true;
    
}


void
DAUSensor::
fromDOMElement(const xercesc::DOMElement* node)
throw(InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);
}
