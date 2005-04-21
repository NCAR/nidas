/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#include <DOMObjectFactory.h>
#include <DynamicLoader.h>
#include <atdUtil/Logger.h>

using namespace dsm;

DOMable* DOMObjectFactory::createObject(const std::string& classname)
	throw(atdUtil::Exception)
{
    dom_object_ctor_t* ctor;
    ctor = (dom_object_ctor_t*) DynamicLoader::getInstance()->lookup(
			    std::string("create") + classname);
#ifdef DEBUG
    atdUtil::Logger::getInstance()->log(LOG_INFO,
    	"creating: %s",classname.c_str());
#endif
    return ctor();
}
