/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLEINPUTSTREAM_H
#define DSM_RAWSAMPLEINPUTSTREAM_H

#include <SampleInput.h>
#include <Datagrams.h>

namespace dsm {

class RawSampleInputStream: public SampleInputStream
{
public:

    RawSampleInputStream(IOChannel* iochannel = 0);

    ~RawSampleInputStream();

    SampleInputStream* clone() const { return new RawSampleInputStream(*this); }

    // bool isRaw() const { return true; }

    // int getPseudoPort() const { return RAW_SAMPLE; }


protected:
};

}

#endif
