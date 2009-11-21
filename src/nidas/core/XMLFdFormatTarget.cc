/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/XMLFdFormatTarget.h>

#include <iostream>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring> // memcpy()

using namespace nidas::core;

namespace n_u = nidas::util;

XMLFdFormatTarget::XMLFdFormatTarget(const std::string& n, int f) :
	name(n),fd(f),fDataBuf(0),fIndex(0),fCapacity(1024)
{
    struct stat statbuf;
    _isSocket = !::fstat(fd,&statbuf) && S_ISSOCK(statbuf.st_mode);
    fDataBuf = new XMLByte[fCapacity];
}

XMLFdFormatTarget::~XMLFdFormatTarget()
{
    // std::cerr << "~XMLFdFormatTarget()" << std::endl;
    delete [] fDataBuf;
}
                                                                                
void XMLFdFormatTarget:: flush() throw(n_u::IOException)
{
    XMLByte* eob = fDataBuf + fIndex;
    for (XMLByte* bp = fDataBuf; bp < eob; ) {
	int l;
        if (_isSocket) {
            if ((l = ::send(fd,bp,eob-bp,MSG_NOSIGNAL)) < 0)
                throw n_u::IOException(name,"send",errno);
        }
        else {
            if ((l = ::write(fd,bp,eob-bp)) < 0)
                throw n_u::IOException(name,"write",errno);
        }
	bp += l;
    }
    fIndex = 0;
    return;
}
                                                                                
void  XMLFdFormatTarget::writeChars(const XMLByte *const toWrite,
	const unsigned int count, xercesc::XMLFormatter *const )
	    throw(n_u::IOException)
{
    if (count) {
	insureCapacity(count);
	memcpy(&fDataBuf[fIndex], toWrite, count * sizeof(XMLByte));
	fIndex += count;
    }
    return;
}
void  XMLFdFormatTarget::insureCapacity(unsigned int count) throw(n_u::IOException) {
    if (fIndex + count <= fCapacity) return;
									    
    flush();

    // no chars left in fDataBuf after a flush
									    
    if (count > fCapacity) {
	XMLByte* tmpptr = new XMLByte[count];
	delete [] fDataBuf;
	fDataBuf = tmpptr;
	fCapacity = count;
    }
}
