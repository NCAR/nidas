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
     * Return a pointer to a symbol in the program itself.  Throws an
     * exception if the lookup fails.
     */
    void *lookup(const std::string& name) throw(nidas::util::Exception);

    /**
     * Return a pointer to a symbol from the given library.  If the library
     * path is absolute (begins with a forward slash), then it is opened as
     * is.  Otherwise it is appended to the library install path built in
     * at compile time.  This method throws an exception if the library
     * could not be loaded or the symbol could not be found in the library.
     * Note that if the library is loaded with dlopen(), then it will never
     * be unloaded, even if the symbol is not found.  If the given library
     * name is empty, this call is equivalent to lookup() above.
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


    void* _defhandle;
    static DynamicLoader* _instance;
    static nidas::util::Mutex _instanceLock;
};

}}	// namespace nidas namespace core

#endif
