/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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
    atdUtil::Logger::getInstance()->log(LOG_INFO,
    	"creating: %s",classname.c_str());

    return ctor();
}
