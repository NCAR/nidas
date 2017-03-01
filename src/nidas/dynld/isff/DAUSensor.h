// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2017, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_ISFF_DAUSENSOR_H
#define NIDAS_DYNLD_ISFF_DAUSENSOR_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

using nidas::dynld::DSMSerialSensor;
using nidas::util::IOException;
using nidas::util::InvalidParameterException;

class DAUSensor: public DSMSerialSensor
{

public:

    DAUSensor();

    ~DAUSensor();
    
    void init() throw(InvalidParameterException);

    void
    addSampleTag(SampleTag* stag) throw(InvalidParameterException);

    bool
    process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void
    fromDOMElement(const xercesc::DOMElement* node)
	throw(InvalidParameterException);

protected:
    const nidas::util::EndianConverter* _cvtr;

private:
    dsm_time_t _prevTimeTag;
        
    //array of len 50 prev data
    unsigned char _prevData[50];
    
    int _prevOffset;
};

}}}	// namespace nidas namespace dynld namespace isff

#endif // NIDAS_DYNLD_ISFF_TILTSENSOR_H
