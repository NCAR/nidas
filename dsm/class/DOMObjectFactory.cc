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
using namespace std;

/* local utility function to replace occurences of one string with another
 */
namespace {
string replace_util(const string& str,const string& s1, const string& s2) {
    string res = str;
    if (s1.compare(s2))
	for (size_t bi; (bi = res.find(s1)) != string::npos;)
	    res.replace(bi,s1.length(),s2);
    return res;
}
}

DOMable* DOMObjectFactory::createObject(const string& classname)
	throw(atdUtil::Exception)
{
    dom_object_ctor_t* ctor;
    ctor = (dom_object_ctor_t*) DynamicLoader::getInstance()->lookup(
			    string("create_") + 
			    replace_util(classname,"::","_"));
#ifdef DEBUG
    atdUtil::Logger::getInstance()->log(LOG_INFO,
    	"creating: %s",classname.c_str());
#endif
    return ctor();
}

