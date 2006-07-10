//
//              Copyright 2004 (C) by UCAR
//
// Description:
//
#include <nidas/util/Exception.h>
// #include <cerrno>
// #include <iostream>

using std::string;

namespace nidas { namespace util {

string Exception::errnoToString(int err)
{
    if (err == 0) return string();
#ifdef sun
    char *cp = ::strerror(err); 
    return string(cp);
#else
    char msg[256];
    msg[0] = 0;

    /* the linux manpage for strerror_r shows this prototype
     *      int strerror_r(int errnum, char *buf, size_t n);
     * Later in the manpage it mentions the GNU extension:
     *      char *strerror_r(int errnum, char *buf, size_t n);
     * Linux is using the GNU extension, (which does not change buf).
     */

    char* cp = ::strerror_r(err,msg,sizeof msg - 1); 
    // std::cerr << "cp=" << cp << " msg=" << msg << std::endl;
    return string(cp);
#endif
}

}}
