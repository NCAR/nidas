/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SocketInputStream.h>
#include <FileSetInputStream.h>
#include <InputStream.h>

// #include <iostream>

using namespace dsm;
using namespace std;

/* static */
InputStream* InputStreamFactory::createInputStream(atdUtil::Socket& sock)
{
    return new SocketInputStream(sock);
}
                                                                                         
/* static */
InputStream* InputStreamFactory::createInputStream(atdUtil::InputFileSet& fset)
{
    return new FileSetInputStream(fset);
}

InputStream::InputStream(size_t buflen)
{
    head = tail = buffer = new char[buflen];
    eob = buffer + buflen;
}

InputStream::~InputStream()
{
    delete [] buffer;
}

/**
 * Shift data in buffer down, then do a devRead.
 */
size_t InputStream::read() throw(atdUtil::IOException)
{
    size_t l = available();
    // shift down. memmove supports overlapping memory areas
    memmove(buffer,head,l);
    head = buffer;
    tail = head + l;
    l = devRead(tail,eob-tail);
    cerr << "InputStream, devRead =" << l << endl;
    tail += l;
    return l;
}

/**
 * Read available data into user buffer. May return less than len.
 */
size_t InputStream::read(void* buf, size_t len) throw()
{
    size_t l = available();
    if (len < l) l = len;
    memcpy(buf,head,l);
    head += l;
    return l;
}
