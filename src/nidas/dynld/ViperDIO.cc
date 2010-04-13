/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/ViperDIO.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(ViperDIO)

ViperDIO::ViperDIO(): fd(-1),noutputs(0),ninputs(0)
{
}

ViperDIO::~ViperDIO()
{
    try {
        if (fd >= 0) close();
    }
    catch(const n_u::IOException& e) {}
}

void ViperDIO::open() throw(n_u::IOException)
{

    if ((fd = ::open(devName.c_str(),O_RDWR)) < 0)
        throw n_u::IOException(devName,"open",errno);

    if ((noutputs = ::ioctl(fd,VIPER_DIO_GET_NOUT,0)) < 0)
        throw n_u::IOException(devName,"ioctl GET_NOUT",errno);

    if ((ninputs = ::ioctl(fd,VIPER_DIO_GET_NIN,0)) < 0)
        throw n_u::IOException(devName,"ioctl GET_NIN",errno);
}


void ViperDIO::close() throw(n_u::IOException)
{
    // don't bother checking for error.
    if (fd >= 0) ::close(fd);
    fd = -1;
}

void ViperDIO::setOutputs(const n_u::BitArray& which)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.getLength() > noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << noutputs;
        throw n_u::InvalidParameterException(devName,"clearOutputs",ost.str());
    }
    const unsigned char* bp = which.getConstPtr();
    if (::ioctl(fd,VIPER_DIO_SET,bp) < 0)
        throw n_u::IOException(devName,"ioctl VIPER_DIO_SET",errno);
}

void ViperDIO::clearOutputs(const n_u::BitArray& which)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{

    if (which.getLength() > noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << noutputs;
        throw n_u::InvalidParameterException(devName,"clearOutputs",ost.str());
    }
    const unsigned char* bp = which.getConstPtr();
    if (::ioctl(fd,VIPER_DIO_CLEAR,bp) < 0)
        throw n_u::IOException(devName,"ioctl VIPER_DIO_SET",errno);
}

void ViperDIO::setOutputs(const n_u::BitArray& which,const n_u::BitArray& val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.getLength() > noutputs || val.getLength() > noutputs) {
        ostringstream ost;
        ost << "number of output bits is " << noutputs;
        throw n_u::InvalidParameterException(devName,"setOutputs",ost.str());
    }
    unsigned char bits[2];
    bits[0] = *which.getConstPtr();
    bits[1] = *val.getConstPtr();
    if (::ioctl(fd,VIPER_DIO_SET_TO_VAL,bits) < 0)
        throw n_u::IOException(devName,"ioctl VIPER_DIO_SET",errno);
}

n_u::BitArray ViperDIO::getOutputs()
            throw(nidas::util::IOException)
{
    unsigned char bits;
    if (::ioctl(fd,VIPER_DIO_GET_DOUT,&bits) < 0)
        throw n_u::IOException(devName,"ioctl VIPER_DIO_GET_DOUT",errno);
    n_u::BitArray b(8);
    b.setBits(0,8,bits);
    return b;
}

n_u::BitArray ViperDIO::getInputs()
            throw(nidas::util::IOException)
{
    unsigned char bits;
    if (::ioctl(fd,VIPER_DIO_GET_DIN,&bits) < 0)
        throw n_u::IOException(devName,"ioctl VIPER_DIO_GET_DIN",errno);
    n_u::BitArray b(8);
    b.setBits(0,8,bits);
    return b;
}

void ViperDIO::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
}

