/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_DYNLD_RAWSAMPLEINPUTSTREAM_H
#define NIDAS_DYNLD_RAWSAMPLEINPUTSTREAM_H

#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/Datagrams.h>

namespace nidas { namespace dynld {

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

    RawSampleInputStream* clone(IOChannel*);

    virtual ~RawSampleInputStream();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:
};

}}	// namespace nidas namespace core

#endif
