// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
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

#include <nidas/dynld/IR104_Relays.h>
#include <nidas/core/UnixIODevice.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(IR104_Relays)

IR104_Relays::IR104_Relays():DSMSensor(),_noutputs(0),_ninputs(0)
{
    // open read/write
    setDefaultMode(O_RDWR);
}

IR104_Relays::~IR104_Relays()
{
}

IODevice* IR104_Relays::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}   
        
SampleScanner* IR104_Relays::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    return new DriverSampleScanner();
}
    
void IR104_Relays::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    if ((_noutputs = ::ioctl(getReadFd(),IR104_GET_NOUT,0)) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl GET_NOUT",errno);

    if ((_ninputs = ::ioctl(getReadFd(),IR104_GET_NIN,0)) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl GET_NIN",errno);
}

bool IR104_Relays::process(const Sample* insamp,std::list<const Sample*>& results)
            throw()
{
    // data is a 3 byte array, first byte containing the 
    // settings of relays 0-7.
    if (insamp->getDataByteLength() != 3) return false;

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(getId()+1);
    float *fp = osamp->getDataPtr();

    const unsigned char* ip =
            (const unsigned char*)insamp->getConstVoidDataPtr();

    // There are 20 relays. A 20 bit integer can be converted
    // to a float without loss of digits.
    unsigned int relays = 0;
    for (unsigned int i = 0; i < insamp->getDataByteLength(); i++)
        relays += *ip++ << i*8;

    *fp = (float) relays;

    results.push_back(osamp);

    return true;

}

void IR104_Relays::setOutputs(const n_u::BitArray& which)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.getLength() != _noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << _noutputs;
        throw n_u::InvalidParameterException(getDeviceName(),"clearOutputs",ost.str());
    }
    const unsigned char* bp = which.getConstPtr();
    if (::ioctl(getReadFd(),IR104_SET,bp) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl IR104_SET",errno);
}

void IR104_Relays::clearOutputs(const n_u::BitArray& which)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{

    if (which.getLength() != _noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << _noutputs;
        throw n_u::InvalidParameterException(getDeviceName(),"clearOutputs",ost.str());
    }
    const unsigned char* bp = which.getConstPtr();
    if (::ioctl(getReadFd(),IR104_CLEAR,&bp) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl IR104_SET",errno);
}

void IR104_Relays::setOutputs(const n_u::BitArray& which,const n_u::BitArray& val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.getLength() != _noutputs || val.getLength() != _noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << _noutputs;
        throw n_u::InvalidParameterException(getDeviceName(),"setOutputs",ost.str());
    }
    unsigned char bits[6];
    memcpy(bits,which.getConstPtr(),3);
    memcpy(bits+3,val.getConstPtr(),3);
    if (::ioctl(getReadFd(),IR104_SET_TO_VAL,bits) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl IR104_SET",errno);
}

n_u::BitArray IR104_Relays::getOutputs()
            throw(nidas::util::IOException)
{
    unsigned char bits[3];
    if (::ioctl(getReadFd(),IR104_GET_DOUT,&bits) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl IR104_GET_DOUT",errno);
    n_u::BitArray b(_noutputs);
    for (int i = 0; i < 3; i++) {
        b.setBits(i*8,std::min((i+1)*8,_noutputs),bits[i]);
    }
    return b;
}

n_u::BitArray IR104_Relays::getInputs()
            throw(nidas::util::IOException)
{
    unsigned char bits[3];
    if (::ioctl(getReadFd(),IR104_GET_DIN,&bits) < 0)
        throw n_u::IOException(getDeviceName(),"ioctl IR104_GET_DIN",errno);
    n_u::BitArray b(_ninputs);
    for (int i = 0; i < 3; i++) {
        b.setBits(i*8,std::min((i+1)*8,_ninputs),bits[i]);
    }
    return b;
}

