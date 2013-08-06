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

#ifndef NIDAS_CORE_PHYSCONSTANTS_H
#define NIDAS_CORE_PHYSCONSTANTS_H

namespace nidas { namespace core {

const double MS_PER_KNOT = 1852.0 / 3600.0;

/*
 * 6894.76 Pa/psi * 1 mbar/100Pa = 68.9476 mbar/psi
 */
const float MBAR_PER_PSI = 68.9476;

const float MBAR_PER_KPA = 10.0;

const float KELVIN_AT_0C = 273.15;

}}	// namespace nidas namespace core

#endif

