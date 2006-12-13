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
    	elemConst(e),elemNonConst(0),
	nodename((const char*)XMLStringConverter(e->getNodeName()))
    {
    }

    XDOMElement(xercesc::DOMElement*e) :
    	elemConst(e),elemNonConst(e),
	nodename((const char*)XMLStringConverter(e->getNodeName()))
    {
    }

    const std::string& getAttributeValue(const std::string& aname) {
	std::map<std::string,std::string>::const_iterator ai = attrs.find(aname);
	if (ai == attrs.end()) {
	    XMLStringConverter cname(aname.c_str());

	    // returns empty string if attribute does not have a
	    // specified or default value.
	    XMLStringConverter aval(elemConst->getAttribute((const XMLCh*)cname));

	    std::pair<const std::string,const std::string>
		    p(aname,std::string((const char*)aval));
	    attrs.insert(p);
	    ai = attrs.find(aname);
	}
	return ai->second;
    }
    void setAttributeValue(const std::string& name,const std::string& val)
    {
        XMLStringConverter aname(name);
        XMLStringConverter aval(val);
        if (elemNonConst)
            elemNonConst->setAttribute((const XMLCh*)aname,(const XMLCh*)aval);
        attrs[name] = val;
    }
    const std::string& getNodeName() const { return nodename; }

    const xercesc::DOMElement* getElement() const { return elemConst; }

private:
    const xercesc::DOMElement* elemConst;
    xercesc::DOMElement* elemNonConst;
    std::map<std::string,std::string> attrs;
    std::string nodename;
};

/**
 * Class providing convienence methods to access the string
 * attributes of a DOMAttr.
 */
class XDOMAttr {
public:
    XDOMAttr(const xercesc::DOMAttr*a) :
    	attr(a),
	name((const char*)XMLStringConverter(a->getName())),
	value((const char*)XMLStringConverter(a->getValue()))
    {
    }
    const std::string& getName() const { return name; }
    const std::string& getValue() const { return value; }
protected:
    const xercesc::DOMAttr* attr;
    std::string name;
    std::string value;
};

}}	// namespace nidas namespace core

#endif
