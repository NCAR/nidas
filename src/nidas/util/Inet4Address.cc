//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#include <nidas/util/Inet4Address.h>
#include <nidas/util/Logger.h>

#include <netdb.h>
#include <cctype>	// isdigit
#include <cstring>      // memset

// #define DEBUG
#include <iostream>
#include <cassert>
#include <vector>

using namespace nidas::util;
using namespace std;

Inet4Address::Inet4Address()
{
  std::memset(&_inaddr,0,sizeof(_inaddr));
}

Inet4Address::Inet4Address(const struct in_addr* a):
	_inaddr(*a)
{
}

Inet4Address::Inet4Address(unsigned int a)
{
    _inaddr.s_addr = htonl(a);
}

string Inet4Address::getHostAddress() const {
    char caddr[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET,&_inaddr,caddr,sizeof caddr))
	return "inet_ntop error";	// unlikely
    return caddr;
}

string Inet4Address::getHostName() const throw() {
    return getHostName(*this);
}

/* static */
string Inet4Address::getHostName(const Inet4Address& addr) throw()
{

    if (addr._inaddr.s_addr == INADDR_ANY) {
	return addr.getHostAddress();
    }

    struct sockaddr_in sockaddr;
    memset(&sockaddr,0,sizeof(sockaddr));
    sockaddr.sin_addr = addr.getInAddr();
    sockaddr.sin_port = 0;
    sockaddr.sin_family = AF_INET;

    char hostname[256];

    // man page for getnameinfo says:
    //  NI_NAMEREQD
    //     If  set, then an error is returned if the hostname cannot be determined.
    // NI_NUMERICHOST: If set, then the numeric form of the hostname is returned.
    //  (When not set, this will still happen in case the node’s name cannot be
    //  determined.)
    //
    // The second statement about NI_NUMERICHOST not being set
    // seems to be incorrect, at least in Fedora 10. If flags is 0,
    // i.e. neither NI_NAMEREQD nor NI_NUMERICHOST are set, EAI_AGAIN
    // is returned and hostname is unchanged if the node name cannot
    // be determined.
    //
    // Maybe this depends on what the DNS server returns?
    //
    // Also, sometimes if the name cannot be determined, valgrind
    // reports an uninitialized variable error, no matter how much
    // memset-ing of arguments we do.
    // http://sourceware.org/ml/glibc-bugs/2009-07/msg00022.html

    int flags = 0;
    // int flags = NI_NUMERICHOST;
    // int flags = NI_NAMEREQD | NI_NUMERICHOST;

#ifdef DEBUG
    cerr << "sizeof(struct sockaddr)=" << sizeof(struct sockaddr) <<
        " sizeof(struct sockaddr_in)=" << sizeof(struct sockaddr_in) <<
        " sizeof(sockaddr)=" << sizeof(sockaddr) << endl;
    cerr << "sizeof(sin_addr)=" << sizeof(sockaddr.sin_addr) << endl;
#endif

    int result;
    if ((result = getnameinfo((const struct sockaddr*)&sockaddr,sizeof(sockaddr),
        hostname,sizeof(hostname),0,0,flags)) != 0)  {
        string addrstr = addr.getHostAddress();
#ifdef DEBUG
        cerr << "getnameinfo, addr=\"" << addrstr <<
            "\", result=" << gai_strerror(result) << endl;
#endif
        if (result == EAI_SYSTEM) {
            int err = errno;
            ELOG(("getnameinfo: ") << addrstr << ": " << gai_strerror(result) << ": " <<
                Exception::errnoToString(err));
        }
        else if (result != EAI_AGAIN)
            WLOG(("getnameinfo: ") << addrstr << ": " << gai_strerror(result));
        return addrstr;
    }
    return hostname;
}

/* static */
list<Inet4Address> Inet4Address::getAllByName(const string& hostname)
  	throw(UnknownHostException)
{

    list<Inet4Address> addrlist;
    // Treat a zero-length hostname as 0.0.0.0, INADDR_ANY
    if (hostname.length() == 0) {
        Inet4Address iaddr(INADDR_ANY);
        addrlist.push_back(iaddr);
        return addrlist;
    }

    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    // If you leave socktype at 0, you typically get 3 entries returned
    // for each address, one for each of SOCK_STREAM(1),
    // SOCK_DGRAM(2), and SOCK_RAW(3). We'll limit it to SOCK_STREAM.
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;

    struct addrinfo *addrinfo = 0;
    int result;

    // In some circumstances, if the address cannot be determined,
    // valgrind reports an uninitialized variable error, no matter
    // how much memset-ing of arguments we do.
    // http://sourceware.org/ml/glibc-bugs/2009-07/msg00022.html

    if ((result = getaddrinfo(hostname.c_str(),NULL,&hints,&addrinfo)) != 0)
        throw UnknownHostException(hostname + ": " + gai_strerror(result));

    for (struct addrinfo* aip = addrinfo; aip != NULL; aip = aip->ai_next) {
        if (aip->ai_family == AF_INET) {
            Inet4Address iaddr(&((struct sockaddr_in*)aip->ai_addr)->sin_addr);
            addrlist.push_back(iaddr);
        }
    }
    freeaddrinfo(addrinfo);
    return addrlist;
}

/* static */
Inet4Address Inet4Address::getByName(const string& hostname)
  	throw(UnknownHostException)
{
    list<Inet4Address> addrs = getAllByName(hostname);
    if (addrs.size() > 0) return addrs.front();
    else throw UnknownHostException(hostname);
}

/*
 * How many leading bits match in the two addresses.
 * We compute it by taking the xor and shifting
 * right until it is zero.
 */
int Inet4Address::bitsMatch(const Inet4Address& x) const throw()
{
    unsigned int addr1 = ntohl(_inaddr.s_addr);
    unsigned int addr2 = ntohl(x._inaddr.s_addr);
    unsigned int match = addr1 ^ addr2;
    int i;
    for (i = 32; match > 0 ; i--) match >>= 1;
    return i;
}
