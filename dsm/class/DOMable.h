
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$


*/

#ifndef DSM_DOMABLE_H
#define DSM_DOMABLE_H

#include <atdUtil/InvalidParameterException.h>

#include <DOMObjectFactory.h>
#include <XDOM.h>
#include <XMLStringConverter.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

namespace dsm {

/**
 * Interface of an object that can be instantiated from a DOM element,
 * via the fromDOMElement method, or that can be serialized into a DOM,
 * via the toDOMParent/toDOMElement method.
 */
class DOMable {
public:

    /**
     * Virtual destructor.
     */
    virtual ~DOMable() {}

    /**
     * Initialize myself from a xercesc::DOMElement.
     */
    virtual void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException) = 0;

    /**
     * Create a DOMElement and append it to the parent.
     */
    virtual xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException) = 0;

    /**
     * Add my content into a DOMElement.
     */
    virtual xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException) = 0;

    static const XMLCh* getNamespaceURI() {
	if (!namespaceURI) namespaceURI =
		xercesc::XMLString::transcode(
		        "http://www.eol.ucar.edu/daq");
        return namespaceURI;
    }

private:
    static XMLCh* namespaceURI;

};

}

/**
 * Define "creator" function, an extern "C" function that invokes
 * a public no-arg constructor for a class.
 * @param NS namespace of class.
 * @param CLASSNAME Name of class, which must be a subclass of
 *	DOMable.
 *
 * The purpose here is to define a function that we can
 * lookup by name with DynamicLoader::lookup(string).
 * Since we don't want to know how C++ mangles namespace names,
 * this C function must not be in a namespace.  The class
 * may be in a namespace though, so we do our own crude name
 * mangling in order to keep creator functions for separate
 * namespaces distinct, by substituting underscores for "::".
 *
 * Example:
 * Definition of class myNS::MyClass, and extern "C" creator function:
 *
 * namespace myNS {
 *     class MyClass : public DOMable {
 *     public:
 *         MyClass() { ... }		// no-arg ctor
 *     };
 * }
 * CREATOR_FUNCTION_NS(myNS,MyClass)
 *
 * This defines a C function called create_myNS_MyClass()
 * which invokes the myNS::MyClass() no arg constructor.
 *
 * To create a pointer to a new instance of myNS::MyClass by name,
 * then do:
 *
 * DOMable* newobj = DOMObjectFactory::createObject("myNS::MyClass");
 *
 * This does the same name mangling by prepending "create_" to the
 * string argument, converts all "::" to '_' and does:
 *
 * DynamicLoader::lookup("create_myNS_MyClass");
 *
 * which should find the extern "C" function that was defined with
 * CREATOR_FUNCTION_NS.
 *
 */
#define CREATOR_FUNCTION_NS(NS,CLASSNAME) \
extern "C" {\
    dsm::DOMable* create_##NS##_##CLASSNAME()\
    {\
	return new NS::CLASSNAME();\
    }\
}

/**
 * Same as above, but the default namespace is "dsm".
 * This will define two C functions, one called
 * create_CLASSNAME, where CLASSNAME is the value of
 * the argument and another called create_dsm_CLASSNAME(),
 * both of which invoke the dsm::CLASSNAME() constructor.
 */
#define CREATOR_FUNCTION(CLASSNAME) \
    CREATOR_FUNCTION_NS(dsm,CLASSNAME) \
extern "C" {\
    dsm::DOMable* create_##CLASSNAME()\
    {\
	return new dsm::CLASSNAME();\
    }\
}

/**
 * Similar macro to CREATOR_FUNCTION_NS, except that
 * this used for a two tiered namespace, like: nsA::nsB::myClass.
 * Since I don't know of a way for the C pre-processor to convert
 * "::" to "_" within its arguments, we must pass the namespace
 * names separately, and build the extern "C" name by concatenating
 * with "_".
 */
#define CREATOR_FUNCTION_NS2(NS1,NS2,CLASSNAME) \
extern "C" {\
    dsm::DOMable* create_##NS1##_##NS2##_##CLASSNAME()\
    {\
	return new NS1::NS2::CLASSNAME();\
    }\
}


#endif
