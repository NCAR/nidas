// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#include "SensorCatalog.h"

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SensorCatalog::SensorCatalog(): _sensors()
{
}

SensorCatalog::SensorCatalog(const SensorCatalog& x):
    DOMable(),
    _sensors(x._sensors)
{
}

SensorCatalog::~SensorCatalog()
{
}

SensorCatalog& SensorCatalog::operator=(const SensorCatalog& rhs)
{
    if (&rhs != this) {
        *(DOMable*) this = rhs;
        _sensors = rhs._sensors;
    }
    return *this;
}

const xercesc::DOMElement* SensorCatalog::find(const string& id) const
{
    map<string,xercesc::DOMElement*>::const_iterator mi =
        _sensors.find(id);
    if (mi != _sensors.end()) return mi->second;
    return 0;
}

void SensorCatalog::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    
    if (xnode.getNodeName() != "sensorcatalog")
	    throw n_u::InvalidParameterException(
		    "SensorCatalog::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();
	// cerr << "SensorCatalog: child element name=" << elname << endl;

	if (elname == "serialSensor" ||
	    elname == "arincSensor" ||
            elname == "irigSensor" ||
            elname == "lamsSensor" ||
            elname == "socketSensor" ||
	    elname == "sensor") {
	    const string& id = xchild.getAttributeValue("ID");
	    if(id.length() > 0) {
		map<string,xercesc::DOMElement*>::iterator mi =
			_sensors.find(id);
		if (mi != _sensors.end() && mi->second != (xercesc::DOMElement*)child)
		    throw n_u::InvalidParameterException(
			"SensorCatalog::fromDOMElement",
			"duplicate sensor in catalog, ID",id);
		_sensors.insert(std::make_pair(id,(xercesc::DOMElement*)child));

		/*
		cerr << "sensorCatalog.size=" << size() << endl;
		for (mi = begin(); mi != _sensors.end(); ++mi)
		    cerr << "map:" << mi->first << " " << hex << mi->second <<
		    	dec << endl;
		*/
	    }
        }
	else throw n_u::InvalidParameterException(
			"SensorCatalog::fromDOMElement",
			"unrecognized element",elname);
    }
}

