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

#include "ServiceCatalog.h"

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ServiceCatalog::ServiceCatalog(): _services()
{
}

ServiceCatalog::ServiceCatalog(const ServiceCatalog& x):
    DOMable(),
    _services(x._services)
{
}

ServiceCatalog::~ServiceCatalog()
{
}

ServiceCatalog& ServiceCatalog::operator=(const ServiceCatalog& rhs)
{
    if (&rhs != this) {
        *(DOMable*) this = rhs;
        _services = rhs._services;
    }
    return *this;
}

const xercesc::DOMElement* ServiceCatalog::find(const std::string& id) const
{
    std::map<std::string,xercesc::DOMElement*>::const_iterator mi =
        _services.find(id);
    if (mi != _services.end()) return mi->second;
    return 0;
}

void ServiceCatalog::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    
    if (xnode.getNodeName() != "servicecatalog")
	    throw n_u::InvalidParameterException(
		    "ServiceCatalog::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();
	// cerr << "ServiceCatalog: child element name=" << elname << endl;

	if (elname == "service") {
	    const string& id = xchild.getAttributeValue("ID");
	    if(id.length() > 0) {
		map<string,xercesc::DOMElement*>::iterator mi =
			_services.find(id);
		if (mi != _services.end() && mi->second != (xercesc::DOMElement*)child)
		    throw n_u::InvalidParameterException(
			"ServiceCatalog::fromDOMElement",
			"duplicate service in catalog, ID",id);
		_services.insert(make_pair(id,(xercesc::DOMElement*)child));

		/*
		cerr << "serviceCatalog.size=" << size() << endl;
		for (mi = begin(); mi != _services.end(); ++mi)
		    cerr << "map:" << mi->first << " " << hex << mi->second <<
		    	dec << endl;
		*/
	    }
        }
	else throw n_u::InvalidParameterException(
			"ServiceCatalog::fromDOMElement",
			"unrecognized element",elname);
    }
}

