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

class RawSampleOutputStream: public SampleOutputStream
{
public:

    RawSampleOutputStream(IOChannel* iochan=0);

    virtual ~RawSampleOutputStream();

    RawSampleOutputStream* clone(IOChannel* iochannel);

    bool isRaw() const { return true; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);
protected:

    RawSampleOutputStream(RawSampleOutputStream&,IOChannel*);

private:

    RawSampleOutputStream(const RawSampleOutputStream&);

};

}}	// namespace nidas namespace core

#endif
