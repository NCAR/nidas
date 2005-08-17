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

    /**
     * Constructor.
     * @param iochannel The IOChannel that we use for data input.
     *   RawSampleInputStream will own the pointer to the IOChannel,
     *   and will delete it in ~RawSampleInputStream().
     */
    RawSampleInputStream(IOChannel* iochannel = 0);

    /**
     * Create a copy, but with a new IOChannel.
     */
    RawSampleInputStream(const RawSampleInputStream&x,IOChannel*);

    SampleInputStream* clone(IOChannel*);

    virtual ~RawSampleInputStream();

protected:
};

}

#endif
