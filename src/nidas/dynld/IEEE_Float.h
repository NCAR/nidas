// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#ifndef NIDIS_DYNLD_IEEE_FLOAT_H
#define NIDIS_DYNLD_IEEE_FLOAT_H

#include "DSMSerialSensor.h"
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld {

/**
 * A class for unpacking binary IEEE float values from a record into samples.
 * Binary values can be big or little endian, specified by the "endian" string
 * parameter.
 */
class IEEE_Float: public DSMSerialSensor
{
public:

    IEEE_Float();

    /**
     * Check the endianness, count the number of variables and get the sample tag
     * for this sensor.
     */
    void validate() throw(nidas::util::InvalidParameterException);

    void init() throw(nidas::util::InvalidParameterException);

    /**
     * Convert a raw sample containing IEEE floats into an output sample.
     */
    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

private:

    nidas::util::EndianConverter::endianness _endian;

    const nidas::util::EndianConverter* _converter;

    SampleTag* _sampleTag;

    int _nvars;

    /// no copying
    IEEE_Float(const IEEE_Float& );

    /// no assignment
    IEEE_Float& operator = (const IEEE_Float& );

};

}}	// namespace nidas namespace dynld

#endif
