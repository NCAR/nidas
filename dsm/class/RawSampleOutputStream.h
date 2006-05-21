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

namespace dsm {

class RawSampleOutputStream: public SortedSampleOutputStream
{
public:
    RawSampleOutputStream();

    RawSampleOutputStream(const RawSampleOutputStream&);

    RawSampleOutputStream(const RawSampleOutputStream&,IOChannel*);

    ~RawSampleOutputStream();

    RawSampleOutputStream* clone(IOChannel* iochannel=0) const;

    bool isRaw() const { return true; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);
protected:
};

}

#endif
