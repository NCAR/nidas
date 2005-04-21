/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLEOUTPUTSTREAM_H
#define DSM_RAWSAMPLEOUTPUTSTREAM_H

#include <SampleOutput.h>
#include <Datagrams.h>

namespace dsm {

class RawSampleOutputStream: public SortedSampleOutputStream
{
public:
    RawSampleOutputStream();
    ~RawSampleOutputStream();

    SampleOutput* clone() const { return new RawSampleOutputStream(*this); }

    bool isRaw() const { return true; }

    int getPseudoPort() const { return RAW_SAMPLE; }

protected:
};

}

#endif
