/* -*- mode: c++; c-basic-offset: 4; -*-
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#include <nidas/util/Logger.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/core/DynamicLoader.h>

#include <vector>
#include <sstream>

using std::vector;
using std::string;
using std::ostringstream;

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
const char* DOMObjectFactory::soVersionSuffix = ".so.1";

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
    nidas::core::DOMable* (*ctor)() = 0;

    string qclassname = "nidas_dynld_" + classname;

    // Replace both :: and . with _, supporting
    // C++ and java style namespace and class name delimiters.
    qclassname = replace_util(qclassname,".","_");
    qclassname = replace_util(qclassname,"::","_");
    string entryname = "create_" + qclassname;

    ostringstream errors;

    // Look for symbol in program, and currently loaded libraries.
    try {
        DLOG(("looking for %s in program", entryname.c_str()));
        ctor = (dom_object_ctor_t*) 
            DynamicLoader::getInstance()->lookup(entryname);
        DLOG(("creating: %s", classname.c_str()));
        return ctor();
    }
    catch (const n_u::Exception& e) 
    {
        errors << e.what() << "\n";
    }

    vector<string> libs;

    // Libraries are placed on the search path using a
    // naming convention based on the underscore-converted
    // class name.

    // First a library is searched using a library name of
    //      qclassname + ".so"
    // For example, for a converted class name of
    // "nidas_dynld_myNameSpace_MyObject", add this library to the
    // search path:
    //       nidas_dynld_myNameSpace_MyObject.so
    libs.push_back(qclassname + ".so");

    // Then add libraries based on the converted class name,
    // with a "lib" prefix, successively removing the portion
    // after the last underscore, and appending the soVersionSuffix.
    // For the above example, and a soVersionSuffix of ".so.1",
    // these libraries would be added to the search:
    //   libnidas_dynld_myNameSpace.so.1
    //   libnidas_dynld.so.1
    // Note if the object's class name contains underscores,
    // such as class My_Object, then extra libraries are searched
    // per the above convention.

    for (;;)
    {
        // for some reason valgrind --leak-check=full gives the following warning:
        // "53 bytes in 1 blocks are possibly lost in loss record 11 of 16"
        // and indicates it happens at this next statement.  Not sure why.
        // "libs" and the strings built here are all automatic variables.
        //
        // Here's a bit off the web that may explain it:
        //
        // Note also that if you are using libstdc++ (the standard c++ library for Linux and some BSDs),
        // you should compile your program with GLIBCXX_FORCE_NEW, to disable std::string memory pool
        // optimizations, which look like leaks to valgrind. Remember to turn that back off for your
        // release builds :-).
        //
        // So, we'll ignore the issue, and add a valgrind suppression.
        libs.push_back(string("lib") + qclassname + soVersionSuffix);

        // trim off any trailing string starting with an underscore
        string::size_type uscore;
        if ((uscore = qclassname.rfind("_")) == string::npos) break;
        qclassname = qclassname.substr(0,uscore);

        // don't bother searching libnidas.so.1
        if (qclassname.length() <= 5) break;
    }

    // If a symbol is found in a library, then the library remains
    // loaded.  Symbols in that library can then be found
    // using the the above lookup() without a library name.

    vector<string>::iterator it;
    for (it = libs.begin(); it != libs.end(); ++it)
    {
	try {
	    DLOG(("looking for %s in '%s'", entryname.c_str(), it->c_str()));
	    ctor = (dom_object_ctor_t*) 
		DynamicLoader::getInstance()->lookup(*it, entryname);
            DLOG(("creating: %s", classname.c_str()));
            return ctor();
	}
	catch (const n_u::Exception& e) 
	{
	    errors << e.what() << "\n";
	}
    }
    throw n_u::Exception(errors.str());
}

