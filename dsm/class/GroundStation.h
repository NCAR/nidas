/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-05-16 12:37:43 -0600 (Mon, 16 May 2005) $

    $LastChangedRevision: 2036 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Aircraft.h $
 ********************************************************************

*/

#ifndef DSM_GROUNDSTATION_H
#define DSM_GROUNDSTATION_H

#include <Site.h>

namespace dsm {

/**
 * GroundStation is a sub-class of a measurement Site.
 * A Site contains a collection of Parameters, so most any
 * Parameter specific to an GroundStation can be supported.
 */

class GroundStation : public Site {
public:
    GroundStation();

    virtual ~GroundStation();

    /**
     * Get/Set tail number of this aircraft.
     */
    int getNumber() const;

    void setNumber(int val);


protected:

    mutable int number;

};

}

#endif
