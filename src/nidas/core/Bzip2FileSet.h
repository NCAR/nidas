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
