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

#include <nidas/core/Version.h>
#include <nidas/SvnInfo.h>

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif

const char* nidas::core::Version::_version = SVNREVISION;
