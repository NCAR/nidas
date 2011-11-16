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

/*
possible updates:
    use RTLD_GLOBAL | RTLD_NODELETE flags in dlopen
    RTLD_NODELETE is not necessary if we keep the handles open

    libraries: libnidas_dynld.so, libnidas_isff.so, libnidas_raf.so
        shouldn't need NULL, don't link against libnidas_dynld
        libnidas_dynld is hardcoded,
            libnidas_isff, libnidas_raf are optional:
                XML: <libraries> 
                -l library in runstring (dsm, dsm_server, ck_xml, data_dump, etc), ugh
                If we split out raf and isfs stuff, need to provide backward compat:
                    
                        
    map<string,handle>
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

DynamicLoader::DynamicLoader() throw(n_u::Exception): _defhandle(0)
{
    _defhandle = dlopen(NULL,RTLD_LAZY);
    if (_defhandle == 0) throw n_u::Exception(dlerror());
}

DynamicLoader::~DynamicLoader()
{
    if (_defhandle) dlclose(_defhandle);
    _defhandle = 0;
}

void *
DynamicLoader::
lookup(const std::string& name) throw(n_u::Exception) 
{
    void* sym = dlsym(_defhandle,name.c_str());
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
    void* libhandle = _defhandle;
    std::string libpath = "default";
    // If the library path is empty, use the default handle.
    if (library.length() > 0)
    {
	libpath = library;
	libhandle = dlopen(libpath.c_str(), RTLD_LAZY|RTLD_NODELETE|RTLD_GLOBAL);
	if (libhandle == 0)
	    throw n_u::Exception
		(std::string("DynamicLoader::lookup, library=") +
		 libpath + ": " + dlerror());
    }
    void* sym = dlsym(libhandle,name.c_str());
    const char* lookuperr = dlerror();
    dlclose(libhandle);
    if (!sym) {
        if (lookuperr != 0)
            throw n_u::Exception(
                    std::string("DynamicLoader::lookup, library=") +
                    libpath + ", symbol=\"" + name + "\":" + lookuperr);
    	throw n_u::Exception(
		std::string("DynamicLoader::lookup, library=") +
		libpath + ", symbol=\"" + name + "\": unknown error");
    }
    return sym;
}
