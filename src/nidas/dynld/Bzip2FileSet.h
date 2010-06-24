/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-06-25 11:42:06 -0600 (Thu, 25 Jun 2009) $

    $LastChangedRevision: 4698 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/FileSet.h $
 ********************************************************************

*/

#ifdef HAS_BZLIB_H
#ifndef NIDAS_DYNLD_BZIP2FILESET_H
#define NIDAS_DYNLD_BZIP2FILESET_H

#include <nidas/core/Bzip2FileSet.h>

namespace nidas { namespace dynld {

/**
 * Dynamically loadable nidas::core::Bzip2FileSet.
 */
class Bzip2FileSet: public nidas::core::Bzip2FileSet {

public:

};

}}	// namespace nidas namespace core

#endif
#endif
