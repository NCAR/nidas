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

#include <nidas/core/DynamicLoader.h>

#include <dlfcn.h>

using namespace nidas::core;

namespace n_u = nidas::util;

DynamicLoader* DynamicLoader::_instance = 0;
n_u::Mutex DynamicLoader::_instanceLock;

DynamicLoader* DynamicLoader::getInstance() throw(n_u::Exception) {
    if (!_instance) {
	n_u::Synchronized autosync(_instanceLock);
	if (!_instance) _instance = new DynamicLoader();
    }
    return _instance;
}

DynamicLoader::DynamicLoader() throw(n_u::Exception): _defhandle(0),_libhandles()
{
    // default handle for loading symbols from the program and
    // libraries that are already linked or loaded into the program.
    _defhandle = dlopen(NULL,RTLD_LAZY);
    if (_defhandle == 0) throw n_u::Exception(dlerror());

    // a lookup on library "" will search the default handle.
    _libhandles[""] = _defhandle;
}

DynamicLoader::~DynamicLoader()
{
}

void *
DynamicLoader::
lookup(const std::string& name) throw(n_u::Exception) 
{
    n_u::Synchronized autosync(_instanceLock);

    dlerror();  // clear existing error
    void* sym = dlsym(_defhandle,name.c_str());
    const char* errptr = dlerror();
    if (errptr) {
    	throw n_u::Exception(
                std::string("DynamicLoader::lookup: ") + errptr);
    }
    return sym;
}

void *
DynamicLoader::
lookup(const std::string& library,const std::string& name)
    throw(n_u::Exception) 
{

    // dlerror() returns a readable string describing the most recent error that
    // occurred from dlopen(), dlsym() or dlclose() since the last call to  dlerror(). 
    // That t'aint very compatible with multi-threading. We'll lock a mutex here
    // to have a little more certainty that the string relates to the immediately
    // proceeding dl call by this class from the calling thread. This, of course,
    // has no control of calls to dl routines outside of this class.

    n_u::Synchronized autosync(_instanceLock);

    void* libhandle = _libhandles[library];
    bool prevLoaded = true;

    if (!libhandle) {
        prevLoaded = false;

        libhandle = dlopen(library.c_str(), RTLD_LAZY| RTLD_GLOBAL);
	if (libhandle == 0) {
            // In case other code is calling dl functions, check for non-null dlerror(),
            // since constructing a std::string from a NULL char pointer results in a crash.
            // Hopefully the pointer remains valid here. 
            const char* errptr = dlerror();
            std::string errStr = "unknown error";
            if (errptr) errStr = std::string(errptr);
            // dlerror() string contains the library name (or the program name
            // for the default handle), so we don't need to repeat them in the exc msg.
	    throw n_u::Exception
		(std::string("DynamicLoader::lookup, open: ") + errStr);
        }
        _libhandles[library] = libhandle;
    }

    dlerror();  // clear existing error
    void* sym = dlsym(libhandle,name.c_str());
    const char* errptr = dlerror();
    if (errptr) {
        std::string errStr = errptr;

        // If no symbols have been found in this library so far, unload it.
        if (!prevLoaded && libhandle != _defhandle) {
            dlclose(libhandle);
            _libhandles[library] = 0;
        }
        // dlerror() string contains the library name (or the program name
        // for the default handle) and the symbol name. Don't need to repeat
        // them in the exception message.
        throw n_u::Exception(
                std::string("DynamicLoader::lookup: ") + errStr);
    }
    return sym;
}
