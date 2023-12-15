/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "XMLFdFormatTarget.h"

#include <iostream>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring> // memcpy()

using namespace nidas::core;

namespace n_u = nidas::util;

XMLFdFormatTarget::XMLFdFormatTarget(const std::string& n, int f) :
	name(n),fd(f),fDataBuf(0),fIndex(0),fCapacity(1024),_isSocket(false)
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
                                                                                
void XMLFdFormatTarget:: flush()
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
                                    const XMLSize_t count,
                                    xercesc::XMLFormatter *const )
{
    if (count) {
	insureCapacity(count);
	memcpy(&fDataBuf[fIndex], toWrite, count * sizeof(XMLByte));
	fIndex += count;
    }
    return;
}
void  XMLFdFormatTarget::insureCapacity(unsigned int count) {
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
