// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
 Copyright 2005 UCAR, NCAR, All Rights Reserved

 $LastChangedDate: 2012-04-16 15:40:04 +0000 (Wed, 16 Apr 2012) $

 $LastChangedRevision: 6091 $

 $LastChangedBy: cjw $

 $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/raf/LSZ_HW_EB7032239.cc $

 Taken from the Honeywell Engineering Specification A.4.1 spec. no.
 EB7032239.

 ******************************************************************
 */

#include <nidas/dynld/raf/LSZ_HW_EB7032239.h>

using namespace nidas::dynld::raf;

NIDAS_CREATOR_FUNCTION_NS(raf, LSZ_HW_EB7032239);

double LSZ_HW_EB7032239::processLabel(const int data, sampleType *stype)
{
    int label = data & 0xff;
    int nTargets, mode;

    // Default to single precision. If some label needs to be
    // DOUBLE_ST, change it in the appropriate case.
    *stype = FLOAT_ST;

    if (label >= 0100 && label <= 0164) // This is the case for lightning cell.
    {

    }
    else
    switch (label)
    {
        case 0001:  // BNR - Preamble word.  Page 2.
            nTargets = (data>>23) & 0x3f;
            mode = (data>>8) & 0x7fff;
            return mode;

        default:
            break;
    }
    return _nanf;
}
