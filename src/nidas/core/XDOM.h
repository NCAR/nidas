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

#ifndef NIDAS_CORE_XDOM_H
#define NIDAS_CORE_XDOM_H

#include <nidas/core/XMLStringConverter.h>

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMTypeInfo.hpp>

#include <string>
#include <map>

namespace nidas { namespace core {

/**
 * Wrapper class providing convienence methods to access the
 * string attributes of a DOMElement.
 */
class XDOMElement {
public:
    XDOMElement(const xercesc::DOMElement*e) :
    	_elemConst(e),_elemNonConst(0), _attrs(),
	_nodename(XMLStringConverter(e->getNodeName())),
#if XERCES_VERSION_MAJOR < 3
        _nodetype( e->getTypeInfo() ? (std::string)XMLStringConverter(e->getTypeInfo()->getName()) : "")
#else
        _nodetype( e->getSchemaTypeInfo() ? (std::string)XMLStringConverter(e->getSchemaTypeInfo()->getTypeName()) : "")
#endif
    {
    }

    XDOMElement(xercesc::DOMElement*e) :
    	_elemConst(e),_elemNonConst(e), _attrs(),
        _nodename(XMLStringConverter(e->getNodeName())), _nodetype()
    {
    }

    /**
     * @return an empty string if attribute does not have a
     *  specified or default value.
     */
    const std::string& getAttributeValue(const std::string& aname) {
	std::map<std::string,std::string>::const_iterator ai = _attrs.find(aname);
	if (ai == _attrs.end()) {
	    XMLStringConverter cname(aname.c_str());

	    // returns empty string if attribute does not have a
	    // specified or default value.
	    XMLStringConverter aval(_elemConst->getAttribute((const XMLCh*)cname));

	    std::pair<const std::string,const std::string>
		    p(aname,aval);
	    _attrs.insert(p);
	    ai = _attrs.find(aname);
	}
	return ai->second;
    }
    void setAttributeValue(const std::string& name,const std::string& val)
    {
        XMLStringConverter aname(name);
        XMLStringConverter aval(val);
        if (_elemNonConst)
            _elemNonConst->setAttribute((const XMLCh*)aname,(const XMLCh*)aval);
        _attrs[name] = val;
    }
    const std::string& getNodeName() const { return _nodename; }

    const std::string& getNodeType() const {return _nodetype; }

    const xercesc::DOMElement* getElement() const { return _elemConst; }

private:
    const xercesc::DOMElement* _elemConst;

    xercesc::DOMElement* _elemNonConst;

    std::map<std::string,std::string> _attrs;

    std::string _nodename;

    std::string _nodetype;

    /** No copying */
    XDOMElement(const XDOMElement&);

    /** No assignment */
    XDOMElement& operator=(const XDOMElement&);
};

/**
 * Class providing convienence methods to access the string
 * attributes of a DOMAttr.
 */
class XDOMAttr {
public:
    XDOMAttr(const xercesc::DOMAttr*a) :
    	_attr(a),
	_name(XMLStringConverter(a->getName())),
	_value(XMLStringConverter(a->getValue()))
    {
    }
    const std::string& getName() const { return _name; }
    const std::string& getValue() const { return _value; }
protected:
    const xercesc::DOMAttr* _attr;
    std::string _name;
    std::string _value;

private:
    /** No copying */
    XDOMAttr(const XDOMAttr&);

    /** No assignment */
    XDOMAttr& operator=(const XDOMAttr&);
};

}}	// namespace nidas namespace core

#endif
