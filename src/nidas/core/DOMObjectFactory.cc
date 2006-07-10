/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#include <nidas/core/DOMObjectFactory.h>
#include <nidas/core/DynamicLoader.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* local utility function to replace occurences of one string with another
 */
namespace {
string replace_util(const string& str,const string& s1, const string& s2) {
    string res = str;
    if (s1 != s2)
	for (size_t bi; (bi = res.find(s1)) != string::npos;)
	    res.replace(bi,s1.length(),s2);
    return res;
}
}

DOMable* DOMObjectFactory::createObject(const string& classname)
	throw(n_u::Exception)
{
    // dom_object_ctor_t* ctor = NULL;
    nidas::core::DOMable* (*ctor)();
    try {
	ctor = (dom_object_ctor_t*) DynamicLoader::getInstance()->lookup(
			    string("create_nidas_dynld_") + 
			    replace_util(classname,".","_"));
    }
    catch (const n_u::Exception& e) {
	ctor = (dom_object_ctor_t*) DynamicLoader::getInstance()->lookup(
			    string("create_nidas_dynld_") + 
			    replace_util(classname,"::","_"));
    }
#ifdef DEBUG
    n_u::Logger::getInstance()->log(LOG_INFO,
    	"creating: %s",classname.c_str());
#endif
    return ctor();
}

