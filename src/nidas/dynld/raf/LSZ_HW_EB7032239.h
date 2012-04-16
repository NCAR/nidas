// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
 Copyright 2005 UCAR, NCAR, All Rights Reserved

 $LastChangedDate: 2012-04-16 15:40:04 +0000 (Wed, 16 Apr 2012) $

 $LastChangedRevision: 6091 $

 $LastChangedBy: cjw $

 $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/raf/LSZ_HW_EB7032239.h $

 ******************************************************************
 */

#ifndef NIDAS_DYNLD_RAF_LSZ_HW_EB7032239_H
#define NIDAS_DYNLD_RAF_LSZ_HW_EB7032239_H

#include <nidas/dynld/raf/DSMArincSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * ARINC LSZ label processor.
 *
 * Taken from the Honeywell Engineering Specification A.4.1 spec. no.
 * EB7032239 cage code 55939 "Lightning Sensor"    (pages A-53..79).
 */
class LSZ_HW_EB7032239 : public DSMArincSensor {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    LSZ_HW_EB7032239() {
#ifdef DEBUG
        err("");
#endif
    }

    /** Process all labels from this instrument. */
    double processLabel(const int data, sampleType*);
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
