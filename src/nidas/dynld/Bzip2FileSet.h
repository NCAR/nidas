// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/Config.h> 

#ifdef HAVE_BZLIB_H

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
