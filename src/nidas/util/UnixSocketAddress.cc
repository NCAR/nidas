//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#include <nidas/util/UnixSocketAddress.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>

using namespace nidas::util;
using namespace std;

UnixSocketAddress::UnixSocketAddress(const string& patharg):
	path(patharg)
{

    memset(&sockaddr,0,getSockAddrLen());
    sockaddr.sun_family = getFamily();

    string tmppath = path;
    if (path.length() > 5 && !path.compare(0,5,"unix:"))
    	tmppath = path.substr(5);

    if (tmppath.length() == 0 || tmppath[0] != '/') {
        sockaddr.sun_path[0] = 0;
	unsigned int l = tmppath.length();
	if (l > sizeof(sockaddr.sun_path) - 1) l = 
	    sizeof(sockaddr.sun_path) - 1;
	memcpy(sockaddr.sun_path+1,tmppath.c_str(),l);
    }
    else {
	unsigned int l = tmppath.length();
	if (l > sizeof(sockaddr.sun_path)) l = sizeof(sockaddr.sun_path);
	memcpy(sockaddr.sun_path,tmppath.c_str(),l);
    }
}

UnixSocketAddress::UnixSocketAddress(const struct sockaddr_un* a):
	sockaddr(*a)
{
    assert(a->sun_family == getFamily());
    if (sockaddr.sun_path[0] == '\0') {
	int len;
	for (len = sizeof(sockaddr.sun_path); len > 0; len--)
	    if (sockaddr.sun_path[len-1] != '\0') break;
	    
	ostringstream ost;
	if (len == 0) ost << "null";
	else 
	    for (int i = 1; i < len; i++) {
		if (isprint(sockaddr.sun_path[i])) ost << sockaddr.sun_path[i];
		else ost << " 0x" << hex << setw(2) << setfill('0') <<
		    (unsigned int)sockaddr.sun_path[i] << dec;
	    } 
	// cerr << "abstract sizeof(sockaddr.sun_path)=" <<
	// 	sizeof(sockaddr.sun_path) << " len=" << len << endl;
	path = ost.str();
    }
    else {
	// path = string(sockaddr.sun_path);
	ostringstream ost;
	if (!strlen(sockaddr.sun_path)) ost << "null";
	for (int i = 0; sockaddr.sun_path[i] != '\0'; i++) {
	    // cerr << "i=" << i << " path=" << sockaddr.sun_path[i] << endl;
	    if (isprint(sockaddr.sun_path[i])) ost << sockaddr.sun_path[i];
	    else ost << " 0x" << hex << setw(2) << setfill('0') <<
	    	(unsigned int)sockaddr.sun_path[i] << dec;
	} 
	// cerr << "nonabstract, sizeof(sockaddr.sun_path)=" <<
	// 	sizeof(sockaddr.sun_path) << endl;
	path = ost.str();
    }
#ifdef DEBUG
    cerr << "sockaddr.sun_path[0]=0x" <<
        hex << (int) sockaddr.sun_path[0] << dec <<
        " path=" << path << endl;
#endif
}

/* copy constructor */
UnixSocketAddress::UnixSocketAddress(const UnixSocketAddress& x):
    path(x.path),sockaddr(x.sockaddr)
{
}

/* clone */
UnixSocketAddress* UnixSocketAddress::clone() const 
{
    return new UnixSocketAddress(*this);
}

/* assignment operator */
UnixSocketAddress& UnixSocketAddress::operator=(const UnixSocketAddress& x)
{
    if (this != &x) {
        path = x.path;
        sockaddr = x.sockaddr;
    }
    return *this;
}

std::string UnixSocketAddress::toString() const
{
    std::ostringstream ost;
    ost << "unix:" << path;
    return ost.str();
}

/**
 * Comparator operator for addresses. Useful if this
 * address is a key in an STL map.
 */
bool UnixSocketAddress::operator < (const SocketAddress& x) const {
    if (getFamily() != x.getFamily()) return getFamily() < x.getFamily();
    const UnixSocketAddress& ux =
	    static_cast<const UnixSocketAddress&>(x);
    return path.compare(ux.path) < 0;
}

/**
 * Equality operator for addresses.
 */
bool UnixSocketAddress::operator == (const SocketAddress& x) const {
    if (getFamily() != x.getFamily()) return false;
    const UnixSocketAddress& ux =
	    static_cast<const UnixSocketAddress&>(x);
    return path.compare(ux.path) == 0;
}


