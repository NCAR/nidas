/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef XDOM_H
#define XDOM_H

#include <XMLStringConverter.h>

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMAttr.hpp>

#include <string>
#include <map>

namespace dsm {

/**
 * Wrapper class providing convienence methods to access the
 * string attributes of a DOMElement.
 */
class XDOMElement {
public:
    XDOMElement(const xercesc::DOMElement*e) :
    	elem(e),
	nodename((const char*)XMLStringConverter(e->getNodeName()))
    {
    }

    const std::string& getAttributeValue(const std::string& aname) {
	std::map<std::string,std::string>::iterator ai = attrs.find(aname);
	if (ai == attrs.end()) {
	    XMLStringConverter cname(aname.c_str());
	    XMLStringConverter aval(elem->getAttribute((const XMLCh*)cname));

	    std::pair<const std::string,const std::string>
		    p(aname,std::string((const char*)aval));
	    attrs.insert(p);
	    ai = attrs.find(aname);
	}
	return ai->second;
    }
    const std::string& getNodeName() { return nodename; }

protected:
    const xercesc::DOMElement* elem;
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
    const std::string& getName() { return name; }
    const std::string& getValue() { return value; }
protected:
    const xercesc::DOMAttr* attr;
    std::string name;
    std::string value;
};

}

#endif
