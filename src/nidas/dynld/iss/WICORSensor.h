// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
/*

    Created on: Jun 29, 2011
        Author: granger
*/

#ifndef WICORSENSOR_H_
#define WICORSENSOR_H_

#include "nidas/dynld/raf/UDPSocketSensor.h"

#include <vector>
#include <string>

// POSIX regex
#include <sys/types.h>
#include <regex.h>

namespace nidas { namespace dynld { namespace iss {

class WICORSensor : public virtual nidas::dynld::raf::UDPSocketSensor
{
public:
    WICORSensor();

    virtual
        ~WICORSensor();

    virtual void
        addSampleTag(nidas::core::SampleTag* stag)
        throw (nidas::util::InvalidParameterException);

    virtual bool
        process(const nidas::core::Sample*,
                std::list<const nidas::core::Sample*>& result) throw ();

private:
    std::vector<std::string> _patterns;
    regex_t* _regex;

    /** No copying. */
    WICORSensor(const WICORSensor&);

    /** No assignment. */
    WICORSensor& operator=(const WICORSensor&);
};

} } }

#endif /* WICORSENSOR_H_ */
