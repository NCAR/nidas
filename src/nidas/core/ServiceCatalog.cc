/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-22 13:51:10 -0700 (Mon, 22 May 2006) $

    $LastChangedRevision: 3363 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/core/ServiceCatalog.cc $
 ********************************************************************

*/

#include <nidas/core/ServiceCatalog.h>

#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

ServiceCatalog::ServiceCatalog()
{
}

ServiceCatalog::~ServiceCatalog()
{
}

void ServiceCatalog::fromDOMElement(const DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    
    if (xnode.getNodeName() != "servicecatalog")
	    throw n_u::InvalidParameterException(
		    "ServiceCatalog::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
	// cerr << "ServiceCatalog: child element name=" << elname << endl;

	if (elname == "service") {
	    const string& id = xchild.getAttributeValue("ID");
	    if(id.length() > 0) {
		map<string,DOMElement*>::iterator mi =
			find(id);
		if (mi != end() && mi->second != (DOMElement*)child)
		    throw n_u::InvalidParameterException(
			"ServiceCatalog::fromDOMElement",
			"duplicate service in catalog, ID",id);
		insert(make_pair<string,DOMElement*>(id,(DOMElement*)child));

		/*
		cerr << "serviceCatalog.size=" << size() << endl;
		for (mi = begin(); mi != end(); ++mi)
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

