// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include <nidas/core/Project.h>
#include <nidas/core/XMLConfigWriter.h>
#include <nidas/core/XDOM.h>

#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <iostream>
#include <list>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

XMLConfigWriter::XMLConfigWriter()
    throw (nidas::core::XMLException):
    _filter(0)
{
    setFilter(0);
}


XMLConfigWriter::XMLConfigWriter(const DSMConfig* dsm)
    throw (nidas::core::XMLException):
    _filter(new XMLConfigWriterFilter(dsm))
{
    setFilter(_filter);
}


XMLConfigWriter::~XMLConfigWriter() 
{
    setFilter(0);
    delete _filter;
}

XMLConfigWriterFilter::XMLConfigWriterFilter(const DSMConfig* dsm):
    _dsm(dsm),
    _whatToShow((1<<(xercesc::DOMNode::ELEMENT_NODE-1)) | (1<<(xercesc::DOMNode::DOCUMENT_NODE-1)))
{
}

#if XERCES_VERSION_MAJOR < 3
    short
#else
    xercesc::DOMNodeFilter::FilterAction
#endif
XMLConfigWriterFilter::acceptNode(const xercesc::DOMNode* node) const
{
    XDOMElement xnode((xercesc::DOMElement*)node);
    if ((getWhatToShow() & (1 << (node->getNodeType() - 1))) == 0) {
	// cerr << "getWhatToShow() rejecting node " << xnode.getNodeName() <<endl;
	return xercesc::DOMNodeFilter::FILTER_REJECT;
    }

    string nodename = xnode.getNodeName();

    if (nodename == "aircraft" || nodename == "site") {
	// scan dsms of this aircraft/site. If we find a matching dsm
	// then pass this aircraft/site node on
	xercesc::DOMNode* child;
	for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
	{
	    if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	    XDOMElement xchild((xercesc::DOMElement*) child);
	    if (xchild.getNodeName() == "dsm" &&
	    	acceptDSMNode(child) == xercesc::DOMNodeFilter::FILTER_ACCEPT)
	    	return xercesc::DOMNodeFilter::FILTER_ACCEPT;
	}
	// dsm not found for this aircraft/site
	// cerr << "rejecting " << nodename << " node, name=" <<
	// 	xnode.getAttributeValue("name") << endl;
	return xercesc::DOMNodeFilter::FILTER_REJECT;
    }
    else if (xnode.getNodeName() == "dsm")
        return acceptDSMNode(node);
    else if (xnode.getNodeName() == "server")
	return xercesc::DOMNodeFilter::FILTER_REJECT;
    else if (xnode.getNodeName() == "project")
        return xercesc::DOMNodeFilter::FILTER_ACCEPT;
    else return xercesc::DOMNodeFilter::FILTER_ACCEPT;
}

#if XERCES_VERSION_MAJOR < 3
    short
#else
    xercesc::DOMNodeFilter::FilterAction
#endif
XMLConfigWriterFilter::acceptDSMNode(const xercesc::DOMNode* node) const
{
    XDOMElement xnode((xercesc::DOMElement*) node);
    if (xnode.getNodeName() != "dsm")
	return xercesc::DOMNodeFilter::FILTER_REJECT;	// not a dsm node
    if(!node->hasAttributes()) 
	return xercesc::DOMNodeFilter::FILTER_REJECT;	// no attribute

    const string dsmName = _dsm->getProject()->expandString(xnode.getAttributeValue("name"));
    if (dsmName == _dsm->getName()) {
	// cerr << "accepting dsm node, name=" << dsmName << endl;
	return xercesc::DOMNodeFilter::FILTER_ACCEPT;
    }
    // cerr << "rejecting dsm node, name=" << dsmName << endl;
    return xercesc::DOMNodeFilter::FILTER_REJECT;	// no match
}

void XMLConfigWriterFilter::setWhatToShow(unsigned long val)
{
    _whatToShow = val;
}

unsigned long XMLConfigWriterFilter::getWhatToShow() const
{
    return _whatToShow;
}
