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

#ifndef NIDAS_DYNLD_ISFF_GROUNDSTATION_H
#define NIDAS_DYNLD_ISFF_GROUNDSTATION_H

#include <nidas/core/Site.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * GroundStation is a sub-class of a measurement Site.
 * A Site contains a collection of Parameters, so most any
 * Parameter specific to an GroundStation can be supported.
 */

class GroundStation : public nidas::core::Site {
public:
    GroundStation();

    virtual ~GroundStation();

protected:

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
