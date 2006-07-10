/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAWSAMPLEOUTPUTSTREAM_H
#define NIDAS_DYNLD_RAWSAMPLEOUTPUTSTREAM_H

#include <nidas/dynld/SampleOutputStream.h>

namespace nidas { namespace dynld {

class RawSampleOutputStream: public SortedSampleOutputStream
{
public:
    RawSampleOutputStream();

    RawSampleOutputStream(const RawSampleOutputStream&);

    RawSampleOutputStream(const RawSampleOutputStream&,IOChannel*);

    virtual ~RawSampleOutputStream();

    RawSampleOutputStream* clone(IOChannel* iochannel=0) const;

    bool isRaw() const { return true; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);
protected:
};

}}	// namespace nidas namespace core

#endif
