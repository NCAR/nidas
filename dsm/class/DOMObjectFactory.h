
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_DOMOBJECTFACTORY_H
#define DSM_DOMOBJECTFACTORY_H

#include <DOMable.h>
#include <atdUtil/Exception.h>

namespace dsm {

/**
 * Class that supports creating instances of DOMable objects
 * from a string containing the class name of the object
 * to be created.
 */
class DOMObjectFactory {
public:

    /**
     * Create a DOMable object, given its class name.  In order
     * to be created via this method, an object's class must have
     * the following characteristics:
     * -# Must be derived from class DOMable
     * -# Must have a public, no-arg constructor
     * -# An extern "C" function, with prototype 
     *    "DOMable* createXXXX()" must exist (where XXXX is the 
     *    class name), which returns a pointer to a new instance
     *    of the class.  This function can be defined with
     *    the CREATOR_ENTRY_POINT(className) macro found in
     *    DOMable.h.
     * -# The extern "C" function can be either statically
     *    linked in the program, or in a shareable library.
     *    If it is in a shareable library, the library must
     *    have been previously dynamically loaded. This would
     *    would be the case if the library contained other
     *    symbols previously needed by the program.
     *    This restriction could be removed if we decide
     *    on a fixed name for the library, something like
     *    "libDsm.so".
     *
     * After being created by this method, then the attributes
     * of the object are typically filled in from an XML DOM element
     * via virtual dsm::DOMable::fromDOMElement() method.
     *
     * This method uses the dsm::DynamicLoader class to load and
     * return a pointer to the extern "C" function.
     * 
     */
    static DOMable* createObject(const std::string& classname)
    	throw(atdUtil::Exception);

protected:
    typedef DOMable* dom_object_ctor_t();
};

}

#endif
