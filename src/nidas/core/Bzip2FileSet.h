/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-03-26 22:35:58 -0600 (Thu, 26 Mar 2009) $

    $LastChangedRevision: 4548 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/FileSet.h $
 ********************************************************************

*/

#ifdef HAS_BZLIB_H

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
