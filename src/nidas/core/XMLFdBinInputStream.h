// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_XMLFDBININPUTSTREAM_H
#define NIDAS_CORE_XMLFDBININPUTSTREAM_H
                                                                                
#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <nidas/util/IOException.h>

#include <unistd.h>

#include <iostream>

namespace nidas { namespace core {

/**
 * Implemenation of xercesc::BinInputStream, which reads from a
 * unix file descriptor.
 */
class XMLFdBinInputStream: public xercesc::BinInputStream {
public:

    /**
     * Constructor.
     * @param n name of device - only used when reporting errors.
     * @param f unix file descriptor of device that is already open.
     */
    XMLFdBinInputStream(const std::string& n,int f) : name(n),fd(f),curpos(0),_eof(false) {}
    ~XMLFdBinInputStream()
    {
	// std::cerr << "~XMLFdBinInputStream" << std::endl;
    }

#if XERCES_VERSION_MAJOR < 3
    unsigned int
#else
    XMLFilePos
#endif
    curPos() const { return curpos; }

    /**
     * return number of bytes read, or 0 on EOF.
     */
#if XERCES_VERSION_MAJOR < 3
    unsigned int
#else
    XMLSize_t
#endif
    readBytes(XMLByte* const toFill,
#if XERCES_VERSION_MAJOR < 3
    	const unsigned int maxToRead
#else
    	const XMLSize_t maxToRead
#endif
    )
throw(nidas::util::IOException)
    {
        if (_eof) return 0;
	// std::cerr << "XMLFdBinInputStream reading " << maxToRead << std::endl;
	ssize_t l = ::read(fd,toFill,maxToRead);
	if (l < 0) throw nidas::util::IOException(name,"read",errno);
        for (int i = 0; i < l; i++)
            if (toFill[i] == '\x04') {
                l = i;
                _eof = true;
            }
	curpos += l;
	// std::cerr << "XMLFdBinInputStream read " << std::string((char*)toFill,0,l < 20 ? l : 20) << std::endl;
	// std::cerr << "XMLFdBinInputStream read " << std::string((char*)toFill,0,l) << std::endl;
	// std::cerr << "XMLFdBinInputStream read " << l << std::endl;
	// toFill[l] = 0;
	// toFill[l+1] = 0;
	return l;
    }

#if XERCES_VERSION_MAJOR >= 3
    const XMLCh* getContentType() const
    {
	return 0;
    }
#endif

protected:
    
    std::string name;
    
    int fd;
#if XERCES_VERSION_MAJOR < 3
    unsigned int curpos;
#else
    XMLFilePos curpos;
#endif

    bool _eof;

};

}}	// namespace nidas namespace core

#endif

