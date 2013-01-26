// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

    /**
     * Do we want DSMSensor::process methods at this site to apply
     * variable conversions?  Currently on raf.Aircraft we don't
     * want process methods to apply the conversions. Nimbus does
     * the conversions instead.
     * Starting with the rework_aircraft_cals branch, nidas will apply
     * the cals.
     */
    bool getApplyVariableConversions() const
    {
        return true;
    }

protected:

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
