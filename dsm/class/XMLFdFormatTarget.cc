/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <XMLFdFormatTarget.h>

#include <iostream>

#include <unistd.h>

using namespace dsm;

XMLFdFormatTarget::XMLFdFormatTarget(const std::string& n, int f) :
	name(n),fd(f),fDataBuf(0),fIndex(0),fCapacity(1024)
{
    fDataBuf = new XMLByte[fCapacity];
}

XMLFdFormatTarget::~XMLFdFormatTarget()
{
    // std::cerr << "~XMLFdFormatTarget()" << std::endl;
    delete [] fDataBuf;
}
                                                                                
void XMLFdFormatTarget:: flush() throw(atdUtil::IOException)
{
    XMLByte* eob = fDataBuf + fIndex;
    for (XMLByte* bp = fDataBuf; bp < eob; ) {
	int l;
	// std::cerr << "XMLFdFormatTarget writing: " <<
	// 	std::string((char*)bp,0,(20 > fIndex ? fIndex : 20)) << std::endl;
	if ((l = ::write(fd,bp,eob-bp)) < 0)
	    throw atdUtil::IOException(name,"write",errno);
	// std::cerr << "XMLFdFormatTarget wrote: " << l << std::endl;
	bp += l;
    }
    fIndex = 0;
    return;
}
                                                                                
void  XMLFdFormatTarget::writeChars(const XMLByte *const toWrite,
	const unsigned int count, xercesc::XMLFormatter *const )
	    throw(atdUtil::IOException)
{
    if (count) {
	insureCapacity(count);
	memcpy(&fDataBuf[fIndex], toWrite, count * sizeof(XMLByte));
	fIndex += count;
    }
    return;
}
void  XMLFdFormatTarget::insureCapacity(unsigned int count) throw(atdUtil::IOException) {
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
