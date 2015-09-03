// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
     * Create a DOMable object given its @p classname, by looking up
     * and executing its creator function, typically from a shared library.
     *
     * In order to be created via this method, an object's class must have
     * the following characteristics:
     *
     * -# Must be derived from class DOMable
     * -# An extern "C" function, with prototype 
     *    DOMable* create_XXXX() must exist (where XXXX is based on the 
     *    class name). This function must returns a pointer to a new instance
     *    of the class.  This function can be defined with
     *    the NIDAS_CREATOR_FUNCTION() or NIDAS_CREATOR_FUNCTION_NS()
     *    macros found in DOMable.h.
     * -# The C function is called without arguments, so typically
     *    it must call a no-arg constructor for the class. If the
     *    above macros are used, a no-arg constructor must exist.
     *
     * If it were possible to know how C++ mangles the name of a constructor 
     * then we could dynamically lookup and execute the default constuctor
     * directly. There isn't a standard for name mangling and I don't know of
     * an API for determining a mangled name, so we create and lookup a
     * C creator function using our own mangling scheme.
     *
     * For example, if the following class exists in a shareable library,
     * and is derived from DOMable, with a no-arg constructor:
     *      @c nidas::dynld::MyNameSpace::MyClass
     * there then must also be an extern "C" creator function called:
     *      @c create_nidas_dynld_MyNamespace_MyClass
     * The C function can be defined with this macro:
     *      @c NIDAS_CREATOR_FUNCTION_NS(MyNameSpace,MyClass)
     * The @p classname string argument passed to this createDOMObject() method
     * doesn't contain the leading "nidas::dynld" namespace qualifier, and
     * can be specified in C++ style with "::" delimiters:
     *      @c "MyNameSpace::MyClass"
     * or Java style, with "." delimiters:
     *      @c "MyNameSpace.MyClass"
     *
     * If the class is in the nidas::dynld namespace, the example above
     * becomes:
     *      @c nidas::dynld::MyClass
     * Extern "C" function:
     *      @c create_nidas_dynld_MyClass
     * Macro to define the C function:
     *      @c NIDAS_CREATOR_FUNCTION(MyClass)
     * @p classname argument to this method:
     *      @c "MyClass"
     *
     * The name of the creator function is mangled from the
     * @p classname argument as follows. The given @p classname is converted
     * by appending it to @c nidas_dynld_, and all occurrences of
     * double-colons (::) and periods (.) are replaced with * underscores (_). 
     * Then @c create_ is prepended to the converted class name.
     *
     * The extern "C" function can be either statically linked in the
     * program, or in a shareable library.  createObject() attempts to 
     * resolve the extern "C" function symbol via the following search:
     * -# Lookup the symbol within the program and the currently loaded
     *    dynamic libraries, by calling
     *    DynamicLoader::lookup(const std::string& name).
     * -# If that fails, look for a shared library named after the
     *    converted class name, by passing the converted name with a
     *    @c .so suffix to DynamicLoader::lookup(library,name).
     *    Using the first example above, the dynamic loader would look
     *    for a library called @c nidas_dynld_MyNameSpace_MyClass.so.
     * -# Then look for shared libraries by successively removing trailing
     *    portions of the converted name delimited by underscores, 
     *    prepending @c lib to the name, and appending @c soVersionSuffix.
     *    Using the first example the loader would search the following libraries
     *    in order:
     *    -# @c libnidas_dynld_MyNameSpace.so.1
     *    -# @c libnidas_dynld.so.1
     *    assuming that soVersionSuffix was set to ".so.1" in this class.
     *
     * After being created by this method, then the attributes
     * of the object are typically filled in from an XML DOM element
     * via virtual nidas::core::DOMable::fromDOMElement() method.
     */
    static DOMable* createObject(const std::string& classname)
    	throw(nidas::util::Exception);

    /**
     * Prototype of the creator function.
     */
    typedef DOMable* dom_object_ctor_t();

    /**
     * When searching for libraries based on the class name,
     * add this suffix to the name. Typically something like ".so.1".
     * This may need more thought. The idea is not to use just ".so",
     * without a version number, since that typically refers to symbolic
     * links found only in -devel packages. Would be nice for this
     * to be set somehow by the build system.
     */
    static const char* soVersionSuffix;

private:
};

}}	// namespace nidas namespace core

#endif
