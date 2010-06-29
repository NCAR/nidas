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
#include <nidas/core/DSMTime.h>

namespace nidas {

namespace core {
class IOChannel;
}

namespace dynld {

class RawSampleInputStream: public SampleInputStream
{
public:

    /**
     * Default constructor.
     */
    RawSampleInputStream();

    /**
     * Constructor with a connected IOChannel.
     * @param iochannel The IOChannel that we use for data input.
     *   RawSampleInputStream will own the pointer to the IOChannel,
     *   and will delete it in ~RawSampleInputStream().
     */
    RawSampleInputStream(nidas::core::IOChannel* iochannel);

    /**
     * Create a copy with a different, connected IOChannel.
     */
    RawSampleInputStream* clone(nidas::core::IOChannel*);

    virtual ~RawSampleInputStream();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    /**
     * Create a copy, but with a new IOChannel.
     */
    RawSampleInputStream(RawSampleInputStream&x,nidas::core::IOChannel*);

};

}}	// namespace nidas namespace core

#endif
