/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/IR104_DIO.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

IR104_DIO::IR104_DIO(): _fd(-1),_noutputs(0),_ninputs(0)
{
}

IR104_DIO::~IR104_DIO()
{
    try {
        if (_fd >= 0) close();
    }
    catch(const n_u::IOException& e) {}
}

void IR104_DIO::open() throw(n_u::IOException)
{

    if ((_fd = ::open(_devName.c_str(),O_RDWR)) < 0)
        throw n_u::IOException(_devName,"open",errno);

    if ((_noutputs = ::ioctl(_fd,IR104_GET_NOUT,0)) < 0)
        throw n_u::IOException(_devName,"ioctl GET_NOUT",errno);

    if ((_ninputs = ::ioctl(_fd,IR104_GET_NIN,0)) < 0)
        throw n_u::IOException(_devName,"ioctl GET_NIN",errno);
}


void IR104_DIO::close() throw(n_u::IOException)
{
    // don't bother checking for error.
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
}

void IR104_DIO::setOutputs(const n_u::BitArray& which)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.getLength() != _noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << _noutputs;
        throw n_u::InvalidParameterException(_devName,"clearOutputs",ost.str());
    }
    const unsigned char* bp = which.getConstPtr();
    if (::ioctl(_fd,IR104_SET,bp) < 0)
        throw n_u::IOException(_devName,"ioctl IR104_SET",errno);
}

void IR104_DIO::clearOutputs(const n_u::BitArray& which)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{

    if (which.getLength() != _noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << _noutputs;
        throw n_u::InvalidParameterException(_devName,"clearOutputs",ost.str());
    }
    const unsigned char* bp = which.getConstPtr();
    if (::ioctl(_fd,IR104_CLEAR,&bp) < 0)
        throw n_u::IOException(_devName,"ioctl IR104_SET",errno);
}

void IR104_DIO::setOutputs(const n_u::BitArray& which,const n_u::BitArray& val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.getLength() != _noutputs || val.getLength() != _noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << _noutputs;
        throw n_u::InvalidParameterException(_devName,"setOutputs",ost.str());
    }
    unsigned char bits[6];
    memcpy(bits,which.getConstPtr(),3);
    memcpy(bits+3,val.getConstPtr(),3);
    if (::ioctl(_fd,IR104_SET_TO_VAL,bits) < 0)
        throw n_u::IOException(_devName,"ioctl IR104_SET",errno);
}

n_u::BitArray IR104_DIO::getOutputs()
            throw(nidas::util::IOException)
{
    unsigned char bits[3];
    if (::ioctl(_fd,IR104_GET_DOUT,&bits) < 0)
        throw n_u::IOException(_devName,"ioctl IR104_GET_DOUT",errno);
    n_u::BitArray b(_noutputs);
    for (int i = 0; i < 3; i++) {
        b.setBits(i*8,std::min((i+1)*8,_noutputs),bits[i]);
    }
    return b;
}

n_u::BitArray IR104_DIO::getInputs()
            throw(nidas::util::IOException)
{
    unsigned char bits[3];
    if (::ioctl(_fd,IR104_GET_DIN,&bits) < 0)
        throw n_u::IOException(_devName,"ioctl IR104_GET_DIN",errno);
    n_u::BitArray b(_ninputs);
    for (int i = 0; i < 3; i++) {
        b.setBits(i*8,std::min((i+1)*8,_ninputs),bits[i]);
    }
    return b;
}

