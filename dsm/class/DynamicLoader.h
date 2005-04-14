/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DYNAMIC_LOADER_H
#define DYNAMIC_LOADER_H

#include <atdUtil/ThreadSupport.h>
#include <atdUtil/Exception.h>

namespace dsm {

/**
 * Class providing dynamic loader functionality of
 * system functions dlopen() and dlsym() to look up symbols.
 */
class DynamicLoader {

public:

    /**
     * Get a pointer to the singleton instance of DynamicLoader.
     */
    static DynamicLoader* getInstance() throw(atdUtil::Exception);

    /**
     * Return a pointer to a symbol in the program itself. The
     * program must have been loaded with the -rdynamic option.
     */
    void *lookup(const std::string& name) throw(atdUtil::Exception);

    /**
     * Return a pointer to a symbol from a given library. Do
     * "man dlopen" for information about library paths.
     */
    void *lookup(const std::string& library,const std::string& name)
    	throw(atdUtil::Exception);

private:
    DynamicLoader() throw(atdUtil::Exception);
    ~DynamicLoader();
    void* defhandle;
    static DynamicLoader* instance;
    static atdUtil::Mutex instanceLock;
};

}

#endif
