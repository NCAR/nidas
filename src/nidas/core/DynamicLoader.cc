/* -*- mode: c++; c-basic-offset: 4; -*-
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

DynamicLoader* DynamicLoader::instance = 0;
n_u::Mutex DynamicLoader::instanceLock;

DynamicLoader* DynamicLoader::getInstance() throw(n_u::Exception) {
    if (!instance) {
	n_u::Synchronized autosync(instanceLock);
	if (!instance) instance = new DynamicLoader();
    }
    return instance;
}

DynamicLoader::DynamicLoader() throw(n_u::Exception)
{
    defhandle = dlopen(NULL,RTLD_LAZY);
    if (defhandle == 0) throw n_u::Exception(dlerror());
}

DynamicLoader::~DynamicLoader()
{
    if (defhandle) dlclose(defhandle);
    defhandle = 0;
}

void *
DynamicLoader::
lookup(const std::string& name) throw(n_u::Exception) 
{
    void* sym = dlsym(defhandle,name.c_str());
    const char* lookuperr = dlerror();
    if (lookuperr != 0 && sym == 0)
    	throw n_u::Exception(
		std::string("DynamicLoader::lookup, symbol=\"") +
		name + "\":" + lookuperr);
    return sym;
}

void *
DynamicLoader::
lookup(const std::string& library,const std::string& name)
    throw(n_u::Exception) 
{
    // If the library path is already absolute, use it.  Otherwise
    // prepend the compiled library install directory.
    void* libhandle = defhandle;
    std::string libpath = "default";
    // If the library path is empty, use the default handle.
    if (library.length() > 0)
    {
	libpath = library;
	if (libpath.find("/") != 0)
	{
	    libpath.replace (0, 0, NIDAS_DYNLD_LIBRARY_PATH "/");
	}
	libhandle = dlopen(libpath.c_str(), RTLD_LAZY);
	if (libhandle == 0)
	    throw n_u::Exception
		(std::string("DynamicLoader::lookup, library=") +
		 libpath + ": " + dlerror());
    }
    void* sym = dlsym(libhandle,name.c_str());
    const char* lookuperr = dlerror();
    //dlclose(libhandle);
    if (lookuperr != 0 && sym == 0)
    	throw n_u::Exception(
		std::string("DynamicLoader::lookup, library=") +
		libpath + ", symbol=\"" + name + "\":" + lookuperr);
    return sym;
}
