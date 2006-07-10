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
     * Return a pointer to a symbol in the program itself. The
     * program must have been loaded with the -rdynamic option.
     */
    void *lookup(const std::string& name) throw(nidas::util::Exception);

    /**
     * Return a pointer to a symbol from a given library. Do
     * "man dlopen" for information about library paths.
     */
    void *lookup(const std::string& library,const std::string& name)
    	throw(nidas::util::Exception);

private:
    DynamicLoader() throw(nidas::util::Exception);
    ~DynamicLoader();
    void* defhandle;
    static DynamicLoader* instance;
    static nidas::util::Mutex instanceLock;
};

}}	// namespace nidas namespace core

#endif
