// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-04-22 10:12:41 -0600 (Sun, 22 Apr 2007) $

    $LastChangedRevision: 3836 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/DSC_A2DSensor.h $

    Created on: Jun 29, 2011
        Author: granger
 ******************************************************************
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
