/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/Parameter.h>
#include <nidas/core/Sample.h>	// floatNAN

#include <sstream>
#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

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
    return floatNAN;
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
    throw(n_u::InvalidParameterException)
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
	    if (aname == "type") {
	        if (aval == "float")
			parameter = new ParameterT<float>();
	        else if (aval == "bool")
			parameter = new ParameterT<bool>();
	        else if (aval == "string")
			parameter = new ParameterT<string>();
	        else if (aval == "strings")
			parameter = new ParameterT<string>();
	        else if (aval == "int")
			parameter = new ParameterT<int>();
	        else if (aval == "hex")
			parameter = new ParameterT<int>();
		else throw n_u::InvalidParameterException("parameter",
			aname,aval);

	        parameter->fromDOMElement(node);

		return parameter;
	    }
	}
    }
    throw n_u::InvalidParameterException("parameter",
	    "element","no type attribute found");
}

template<class T>
ParameterT<T>* ParameterT<T>::clone() const
{
    return new ParameterT<T>(*this);
}

template<class T>
void ParameterT<T>::assign(const Parameter& x)
{
    if (type == x.getType()) {
	name = x.getName();
	const ParameterT<T> * xT =
	    dynamic_cast<const ParameterT<T>*>(&x);
	if (xT) values = xT->values;
    }
}

template<class T>
void ParameterT<T>::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
	// get all the attributes of the node

	const string& ptype = xnode.getAttributeValue("type");
	bool oneString = ptype == "string";

	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();

	    if (aname == "name") setName(aval);
	    else if (aname == "value") {
		// get attribute value(s)

		// If type is simply "string", don't break it up.
		if (oneString) {
		    // ugly!
		    ParameterT<string>* strParam =
		    	dynamic_cast<ParameterT<string>*>(this);
		    strParam->setValue(aval);
		}
		else {
		    std::istringstream ist(aval);
		    if (ptype == "hex") ist >> hex;
		    if (ptype == "bool") ist >> boolalpha;
		    T val;
		    for (int i = 0; ; i++) {
			ist >> val;
#ifdef DEBUG
			std::cerr << 
			    "Parameter::fromDOMElement, read, val=" << val <<
			    " eof=" << ist.eof() <<
			    " fail=" << ist.fail() << std::endl;
#endif
			// In case a bool was entered as "0" or "1", turn off boolalpha and try again.
			if (ist.fail()) {
			    ist.clear();
			    ist >> noboolalpha >> val;
			    if (ist.fail())
				throw n_u::InvalidParameterException(
				    "parameter",getName(),aval);
			    ist >> boolalpha;
			}
			setValue(i,val);
			if (ist.eof()) break;
		    }
		}
#ifdef DEBUG
		std::cerr << "Parameter::fromDOMElement, getLength()=" <<
			getLength() << std::endl;
#endif
	    }
	    else if (aname != "type")
		throw n_u::InvalidParameterException(
		    "parameter",aname,aval);
	}
    }
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

