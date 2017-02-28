// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 5; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
/* 
 * Watlow
 * 
 */

#include "WatlowCLS208.h"

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

const n_u::EndianConverter* Watlow::_fromBig = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);


NIDAS_CREATOR_FUNCTION_NS(raf,Watlow)

uint16_t Watlow::crcCheck(unsigned char * input, int messageLength, int start) throw()
{
    //"CRC is started by first preloading a 16 bit register to all 1's"  Manual pg 25    
    uint16_t checksum = 0xffff;
    for ( int i=0; i<=messageLength; i++)
    { 
        uint16_t data_byte =input[start+i];
        checksum = checksum ^ data_byte;
        for (int j =0; j<8;j++)
        {
            if ((checksum %2) ==1)
            {
                 checksum =0xa001^(checksum >>1);//shift bit right, add in 0 at left
                //xor constant from http://docplayer.net/40721506-Cls200-mls300-and-cas200-communications-specification.html
            }
            else
            {
                checksum = checksum >>1;
            }
        }
    }
    return checksum;
 }



bool Watlow::process(const Sample* samp,list<const Sample*>& results) throw()
{

    unsigned char * input = (unsigned char*) samp->getConstVoidDataPtr();
    int16_t data[10];
 
    SampleT<float> * outs1 = getSample<float>(8);
    float *douts1 = outs1->getDataPtr();
    outs1->setTimeTag( samp->getTimeTag());
    outs1->setId(getId()+1);
    memcpy (data,&input[3],18);
    uint16_t checksum = crcCheck(input,18,0);
    if(! (checksum==uint16_t(data[8]))){ return false;}
    for (unsigned int i = 0; i < 8; i++){
        *douts1++ = (float)_fromBig->int16Value( (data[i])) / 10.0;
    }
    results.push_back(outs1);
 
    SampleT<float> * outs2 = getSample<float>(1);
    float *douts2 = outs2->getDataPtr();
    outs2->setTimeTag( samp->getTimeTag());
    outs2->setId(getId()+2);
    memcpy (data,&input[24],4);
    checksum = crcCheck(input, 4, 21);
    if (checksum != uint16_t(data[1])) return false;
    for (unsigned int i = 0; i < 1; i++){
        *douts2++ = (float)_fromBig->int16Value( data[i]) / 10.0;
    }
    results.push_back(outs2);
 
    SampleT<float> * outs3 = getSample<float>(4);
    float *douts3 = outs3->getDataPtr();
    outs3->setTimeTag( samp->getTimeTag());
    outs3->setId(getId()+3);
    memcpy (data,&input[31],10);
    checksum = crcCheck(input, 10, 28);
    if (checksum != uint16_t(data[4])) return false;
    //cout << "Calculated checksum:" << (checksum)<<"Given Checksum:"<<uint16_t(data[4])<< "Calculated Big checksum:" << _fromBig-> uint16Value(checksum)<<"Given Checksum:"<<_fromBig->uint16Value(data[4])  << endl;
    for (unsigned int i = 0; i < 4; i++){
        *douts3++ = (float)_fromBig->uint16Value( data[i]);
    }
    results.push_back(outs3);

    return true;
}

