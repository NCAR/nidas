/* -*- mode: c++; c-basic-offset: 4; -*-
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

#include <vector>
#include <sstream>

using std::vector;
using std::string;
using std::ostringstream;

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* local utility function to replace occurences of one string with another
 */
namespace {
string replace_util(const string& str,const string& s1, const string& s2) {
    string res = str;
    if (s1 != s2)
	for (string::size_type bi; (bi = res.find(s1)) != string::npos;)
	    res.replace(bi,s1.length(),s2);
    return res;
}
}

DOMable* 
DOMObjectFactory::
createObject(const string& classname) throw(n_u::Exception)
{
    n_u::Logger* logger = n_u::Logger::getInstance();
    // dom_object_ctor_t* ctor = NULL;
    nidas::core::DOMable* (*ctor)() = 0;
    // Replace both :: and . with _, just in case either is used.
    string qclassname = "nidas_dynld_" + classname;
    qclassname = replace_util(qclassname,".","_");
    qclassname = replace_util(qclassname,"::","_");
    string entryname = "create_" + qclassname;
    vector<string> libs;
    libs.push_back("");
    libs.push_back(qclassname + ".so");
    string::size_type uscore;
    while ((uscore = libs.back().rfind("_")) != string::npos)
    {
	libs.push_back("lib" + libs.back().substr(0, uscore) + ".so");
	break;
    }
    ostringstream errors;
    vector<string>::iterator it;
    for (it = libs.begin(); !ctor && it != libs.end(); ++it)
    {
	try {
#ifdef DEBUG
	    logger->log(LOG_INFO, "looking for %s in '%s'", 
			entryname.c_str(), it->c_str());
#endif
	    ctor = (dom_object_ctor_t*) 
		DynamicLoader::getInstance()->lookup(*it, entryname);
	}
	catch (const n_u::Exception& e) 
	{
	    errors << e.what() << "\n";
	}
    }
    if (!ctor)
    {
	throw n_u::Exception(errors.str());
    }
#ifdef DEBUG
    logger->log(LOG_INFO, "creating: %s", classname.c_str());
#endif
    return ctor();
}

