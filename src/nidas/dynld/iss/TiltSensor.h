// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#ifndef NIDAS_DYNLD_ISS_TILTSENSOR_H
#define NIDAS_DYNLD_ISS_TILTSENSOR_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld { namespace iss {

using namespace nidas::core;

using nidas::dynld::DSMSerialSensor;
using nidas::util::IOException;
using nidas::util::InvalidParameterException;

/**
 * CXTILT02 2-axis tilt sensor from Crossbow.
 */
class TiltSensor: public DSMSerialSensor
{

public:

    TiltSensor();

    ~TiltSensor();

    void
    addSampleTag(SampleTag* stag) throw(InvalidParameterException);

    bool
    process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void
    fromDOMElement(const xercesc::DOMElement* node)
	throw(InvalidParameterException);

protected:

    int checksumFailures;

    dsm_sample_id_t sampleId;

};

}}}	// namespace nidas namespace dynld namespace iss

#endif // NIDAS_DYNLD_ISS_TILTSENSOR_H
