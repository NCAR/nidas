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

#ifndef NIDAS_CORE_DYNAMIC_LOADER_H
#define NIDAS_CORE_DYNAMIC_LOADER_H

#include <nidas/util/ThreadSupport.h>
#include <nidas/util/Exception.h>

#include <map>

namespace nidas { namespace core {

/**
 * Class providing dynamic loader functionality of
 * system functions dlopen() and dlsym() to look up symbols.
 */
class DynamicLoader {

public:

    /**
     * Get a pointer to the singleton instance of DynamicLoader.
     */
    static DynamicLoader* getInstance() throw(nidas::util::Exception);

    /**
     * Search the main program itself, and its currently loaded
     * libraries for a symbol. Throws an exception if the lookup fails.
     */
    void *lookup(const std::string& name) throw(nidas::util::Exception);

    /**
     * Return a pointer to a symbol from the given library.
     * @ param library: name of the library.
     * @ param name: name of the symbol to look up.
     *
     * If the library name is an empty string, then the libraries linked with
     * the program, and any currently loaded dynamic libraries will
     * be searched.
     * If the library name is absolute (begins with a forward slash),
     * then it is loaded. Otherwise a search for the library is done, using
     * the program's library directory search path, which is controlled
     * by compile time flags, the LD_LIBRARY_PATH environment variable,
     * and ld.so/ldconfig/ld.so.conf.
     * lookup() throws an exception if the library could not be found and
     * loaded or the symbol could not be found in the library.
     * If the symbol is found, the library will remain loaded, and
     * symbols in that library can then be found via lookup(name),
     * or by specifying an emptry string for the library.
     */
    void *lookup(const std::string& library,const std::string& name)
    	throw(nidas::util::Exception);

private:
    DynamicLoader() throw(nidas::util::Exception);

    /** No copy. */
    DynamicLoader(const DynamicLoader&);

    /** No assignment */
    DynamicLoader& operator=(const DynamicLoader&);

    ~DynamicLoader();

    /**
     * Handle, returned by dlopen(), of the program itself, and its
     * linked and dynamically loaded libraries.
     */
    void*  _defhandle;

    /**
     * Handles, by library name, returned by dlopen() of libraries
     * that are currently open, because one or more symbols have
     * been found in them.
     */
    std::map<std::string,void*>  _libhandles;

    static DynamicLoader* _instance;

    static nidas::util::Mutex _instanceLock;

};

}}	// namespace nidas namespace core

#endif
