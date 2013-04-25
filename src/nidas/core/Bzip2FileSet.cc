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

#include <nidas/core/Bzip2FileSet.h>

#ifdef HAVE_BZLIB_H

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

Bzip2FileSet::Bzip2FileSet(): FileSet(new nidas::util::Bzip2FileSet())
{
     _name = "Bzip2FileSet";
}

/* Copy constructor. */
Bzip2FileSet::Bzip2FileSet(const Bzip2FileSet& x):
    	FileSet(x)
{
}

void Bzip2FileSet::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    FileSet::fromDOMElement(node);

    XDOMElement xnode(node);
    // const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
	// get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            // const std::string& aval = attr.getValue();
	    if (aname == "compress") {} // nothing yet
	}
    }
}
#endif

