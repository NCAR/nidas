/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-05-16 12:37:43 -0600 (Mon, 16 May 2005) $

    $LastChangedRevision: 2036 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Aircraft.h $
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
