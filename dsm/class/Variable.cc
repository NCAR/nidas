/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Variable.h>
#include <sstream>

using namespace dsm;
using namespace std;
using namespace xercesc;

Variable::Variable(): converter(0),iscount(false)
{
}

Variable::~Variable()
{
    delete converter;
}

void Variable::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    if (!attr.getName().compare("name"))
		setName(attr.getValue());
	    else if (!attr.getName().compare("longname"))
		setLongName(attr.getValue());
	    else if (!attr.getName().compare("units"))
		setUnits(attr.getValue());
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;

        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

	if (converter) throw atdUtil::InvalidParameterException(getName(),
		"only one child element allowed",elname);

	converter = VariableConverter::createVariableConverter(elname);
	if (!converter) throw atdUtil::InvalidParameterException(getName(),
		"unsupported child element",elname);
	converter->fromDOMElement((DOMElement*)child);
    }
}

DOMElement* Variable::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* Variable::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


