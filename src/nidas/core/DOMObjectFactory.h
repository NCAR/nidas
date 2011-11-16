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

#ifndef NIDAS_CORE_DOMOBJECTFACTORY_H
#define NIDAS_CORE_DOMOBJECTFACTORY_H

#include <nidas/util/Exception.h>

namespace nidas { namespace core {

class DOMable;

/**
 * Class that supports creating instances of DOMable objects from a string
 * containing the class name of the object to be created.  Classes can be
 * dynamically loaded from shared libraries.
 */
class DOMObjectFactory {
public:

    /**
     * Create a DOMable object given its @p classname, possibly by first
     * loading its creator function from a shared library.
     *
     * In order to be created via this method, an object's class must have
     * the following characteristics:
     *
     * -# Must be derived from class DOMable
     * -# Must have a public, no-arg constructor
     * -# An extern "C" function, with prototype 
     *    "DOMable* createXXXX()" must exist (where XXXX is the 
     *    class name), which returns a pointer to a new instance
     *    of the class.  This function can be defined with
     *    the NIDAS_CREATOR_FUNCTION() or NIDAS_CREATOR_FUNCTION_NS()
     *    macros found in DOMable.h.
     * -# The extern "C" function can be either statically
     *    linked in the program, or in a shareable library.
     *    The method attempts to resolve the extern "C" function symbol
     *    with these steps:
     *     -# Lookup the symbol within the program, as in calling 
     *        DynamicLoader::lookup(const std::string& name).
     *     -# Look for a shared library named after the class name,
     *        by passing the library name to DynamicLoader::lookup().
     *     -# Look for shared libraries named after the class namespace.
     *
     * The creator symbol is derived from the class name as follows: The
     * given @p classname is appended to @c nidas_dynld_, and all
     * occurrences of double-colons (::) and periods (.) are replaced with
     * underscores (_).  Thus for the class name @c psql.PSQLSampleOutput,
     * the fully qualified class name becomes @c
     * nidas_dynld_psql_PSQLSampleOutput.  The full class name is appended
     * to @c create_ to generate this C symbol:
     *
     *  @c create_nidas_dynld_psql_PSQLSampleOutput
     *
     * This symbol is first looked for built into the program (the default
     * dlopen() handle), then successive libraries are tried as listed
     * above.  The first library file would be
     * 'nidas_dynld_psql_PSQLSampleOutput.so', in the built-in library
     * install directory.  The next would be 'libnidas_dynld_psql.so'.  The
     * natural progression would be to also check for libnidas_dynld, but
     * right now only the inner-most namespace is added to the search path.
     *
     * The search sequence of library paths allows all of the objects
     * within a particular namespace to be combined into one shared
     * library, which is especially useful when the objects have
     * interdependencies.
     *
     * After being created by this method, then the attributes
     * of the object are typically filled in from an XML DOM element
     * via virtual nidas::core::DOMable::fromDOMElement() method.
     */
    static DOMable* createObject(const std::string& classname)
    	throw(nidas::util::Exception);

protected:
    typedef DOMable* dom_object_ctor_t();
};

}}	// namespace nidas namespace core

#endif
