/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Parameter.h>
#include <sstream>
#include <iostream>

#include <math.h>	// nanf

using namespace dsm;
using namespace std;
using namespace xercesc;

double Parameter::getNumericValue(int i) const
{
    switch (type) {
    case FLOAT_PARAM:
	return static_cast<const ParameterT<float>*>(this)->getValue(i);
	break;
    case INT_PARAM:
	return static_cast<const ParameterT<int>*>(this)->getValue(i);
	break;
    case STRING_PARAM:
	break;
    case BOOL_PARAM:
	return static_cast<const ParameterT<bool>*>(this)->getValue(i);
	break;
    }
    return nanf("");
}

std::string Parameter::getStringValue(int i) const
{
    ostringstream ost;
    switch (type) {
    case FLOAT_PARAM:
	ost << static_cast<const ParameterT<float>*>(this)->getValue(i);
	break;
    case INT_PARAM:
	ost << static_cast<const ParameterT<int>*>(this)->getValue(i);
	break;
    case STRING_PARAM:
	ost << static_cast<const ParameterT<string>*>(this)->getValue(i);
	break;
    case BOOL_PARAM:
	ost << static_cast<const ParameterT<bool>*>(this)->getValue(i);
	break;
    }
    return ost.str();
}

Parameter* Parameter::createParameter(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    Parameter* parameter = 0;
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
	    if (!aname.compare("type")) {
	        if (!aval.compare("float"))
			parameter = new ParameterT<float>();
	        else if (!aval.compare("bool"))
			parameter = new ParameterT<bool>();
	        else if (!aval.compare("string"))
			parameter = new ParameterT<string>();
	        else if (!aval.compare("int"))
			parameter = new ParameterT<int>();
		throw atdUtil::InvalidParameterException("parameter",
			aname,aval);

	        parameter->fromDOMElement(node);

		return parameter;
	    }
	}
    }
    throw atdUtil::InvalidParameterException("parameter",
	    "element","no type attribute found");
}

DOMElement* Parameter::toDOMParent(
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

DOMElement* Parameter::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


