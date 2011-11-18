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
     *    "DOMable* create_XXXX()" must exist (where XXXX is based on the 
     *    class name). This function must returns a pointer to a new instance
     *    of the class.  This function can be defined with
     *    the NIDAS_CREATOR_FUNCTION() or NIDAS_CREATOR_FUNCTION_NS()
     *    macros found in DOMable.h.
     *
     * The name of the creator function is derived from the
     * class name as follows. The given @p classname is appended
     * to @c nidas_dynld_, and all occurrences of double-colons (::)
     * and periods (.) are replaced with * underscores (_).
     * Thus for the class name of @c psql::PSQLSampleOutput or
     * @c psql.PSQLSampleOutput, the converted name becomes
     * @c nidas_dynld_psql_PSQLSampleOutput.
     * The converted name is appended
     * to @c create_ to generate this C symbol:
     *
     *  @c create_nidas_dynld_psql_PSQLSampleOutput
     *
     * The extern "C" function can be either statically
     * linked in the program, or in a shareable library.
     * createObject() attempts to resolve the extern "C" function symbol
     * with these steps:
     * -# Lookup the symbol within the program and the currently loaded
     *    dynamic libraries, by calling
     *    DynamicLoader::lookup(const std::string& name).
     * -# Look for a shared library named after the converted class name,
     *    by passing the converted name with a @c .so suffix
     *    to DynamicLoader::lookup(library,symbol).
     * -# Look for shared libraries by successively removing trailing
     *    portions of the converted name delimited by underscores, 
     *    prepending @c lib to the name, and appending @c soVersionSuffix.
     *
     * Therefore the symbol is first looked for in the program itself
     * and the currently loaded libraries. If the symbol is not found,
     * The next library file that is searched would be
     * 'nidas_dynld_psql_PSQLSampleOutput.so', in the program's library
     * search path. If the symbol is not found, 'libnidas_dynld_psql.so.1'
     * would be searched next, followed by 'libnidas_dynld.so.1'.
     *
     * The search sequence of libraries allows all of the objects
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

    typedef DOMable* dom_object_ctor_t();

    /**
     * When searching for libraries based on the class name,
     * add this suffix to the name. Typically something like ".so.1".
     */
    static const char* soVersionSuffix;

private:
};

}}	// namespace nidas namespace core

#endif
