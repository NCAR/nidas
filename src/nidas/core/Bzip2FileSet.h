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

#ifndef NIDAS_CORE_BZIP2FILESET_H
#define NIDAS_CORE_BZIP2FILESET_H

#include <nidas/core/FileSet.h>
#include <nidas/util/Bzip2FileSet.h>

namespace nidas { namespace core {

/**
 * A FileSet that support bzip2 compression/uncompression.
 */
class Bzip2FileSet: public FileSet {

public:

    Bzip2FileSet();

    /**
     * Clone myself.
     */
    Bzip2FileSet* clone() const
    {
        return new Bzip2FileSet(*this);
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    /**
     * Copy constructor.
     */
    Bzip2FileSet(const Bzip2FileSet& x);

private:
    /**
     * No assignment.
     */
    Bzip2FileSet& operator=(const Bzip2FileSet&);
};

}}	// namespace nidas namespace core

#endif
#endif
