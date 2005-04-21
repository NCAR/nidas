/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <DynamicLoader.h>
#include <dlfcn.h>

using namespace dsm;

DynamicLoader* DynamicLoader::instance = 0;
atdUtil::Mutex DynamicLoader::instanceLock;

DynamicLoader* DynamicLoader::getInstance() throw(atdUtil::Exception) {
    if (!instance) {
	atdUtil::Synchronized autosync(instanceLock);
	if (!instance) instance = new DynamicLoader();
    }
    return instance;
}

DynamicLoader::DynamicLoader() throw(atdUtil::Exception)
{
    defhandle = dlopen(NULL,RTLD_LAZY);
    if (defhandle == 0) throw atdUtil::Exception(dlerror());
}

DynamicLoader::~DynamicLoader()
{
    if (defhandle) dlclose(defhandle);
    defhandle = 0;
}

void *DynamicLoader::lookup(const std::string& name) throw(atdUtil::Exception) 
{
    void* sym = dlsym(defhandle,name.c_str());
    const char* lookuperr = dlerror();
    if (lookuperr != 0 && sym == 0)
    	throw atdUtil::Exception(
		std::string("DynamicLoader::lookup, symbol=\"") +
		name + "\":" + lookuperr);
    return sym;
}

void *DynamicLoader::lookup(const std::string& library,const std::string& name) throw(atdUtil::Exception) 
{
    void* libhandle = dlopen(library.c_str(),RTLD_LAZY);
    if (libhandle == 0)
    	throw atdUtil::Exception(
		std::string("DynamicLoader::lookup, library=") +
		library + ": " + dlerror());
    void* sym = dlsym(libhandle,name.c_str());
    const char* lookuperr = dlerror();
    dlclose(libhandle);
    if (lookuperr != 0 && sym == 0)
    	throw atdUtil::Exception(
		std::string("DynamicLoader::lookup, library=") +
		library + ", symbol=\"" + name + "\":" + lookuperr);
    return sym;
}
