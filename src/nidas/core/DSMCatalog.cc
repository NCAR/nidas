// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "DSMCatalog.h"

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMCatalog::DSMCatalog(): _dsms()
{
}

DSMCatalog::DSMCatalog(const DSMCatalog& x):
    DOMable(),_dsms(x._dsms)
{
}

DSMCatalog::~DSMCatalog()
{
}

DSMCatalog& DSMCatalog::operator=(const DSMCatalog& rhs)
{
    if (&rhs != this) {
        *(DOMable*) this = rhs;
        _dsms = rhs._dsms;
    }
    return *this;
}

const xercesc::DOMElement* DSMCatalog::find(const std::string& id) const
{
    std::map<std::string,xercesc::DOMElement*>::const_iterator mi =
        _dsms.find(id);
    if (mi != _dsms.end()) return mi->second;
    return 0;
}

void DSMCatalog::fromDOMElement(const xercesc::DOMElement* node)
{
    XDOMElement xnode(node);
    
    if (xnode.getNodeName() != "dsmcatalog")
	    throw n_u::InvalidParameterException(
		    "DSMCatalog::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();
	// cerr << "DSMCatalog: child element name=" << elname << endl;

	if (elname == "dsm") {
	    const string& id = xchild.getAttributeValue("ID");
	    if(id.length() > 0) {
		map<string,xercesc::DOMElement*>::iterator mi =
			_dsms.find(id);
		if (mi != _dsms.end() && mi->second != (xercesc::DOMElement*)child)
		    throw n_u::InvalidParameterException(
			"DSMCatalog::fromDOMElement",
			"duplicate sensor in catalog, ID",id);
		_dsms.insert(make_pair(id,(xercesc::DOMElement*)child));

		/*
		cerr << "sensorCatalog.size=" << size() << endl;
		for (mi = begin(); mi != _dsms.end(); ++mi)
		    cerr << "map:" << mi->first << " " << hex << mi->second <<
		    	dec << endl;
		*/
	    }
        }
	else throw n_u::InvalidParameterException(
			"DSMCatalog::fromDOMElement",
			"unrecognized element",elname);
    }
}

