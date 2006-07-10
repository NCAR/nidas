/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAF_AIRCRAFT_H
#define NIDAS_DYNLD_RAF_AIRCRAFT_H

#include <nidas/core/Site.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Aircraft is a sub-class of a measurement Site.
 * A Site contains a collection of Parameters, so most any
 * Parameter specific to an Aircraft can be supported.
 */

class Aircraft : public Site {
public:
    Aircraft();

    virtual ~Aircraft();

    /**
     * Get/Set tail number of this aircraft.
     */
    std::string getTailNumber() const;

    void setTailNumber(const std::string& val);


protected:

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
