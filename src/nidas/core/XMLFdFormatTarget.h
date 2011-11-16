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

#ifndef NIDAS_CORE_XMLFDFORMATTARGET_H
#define NIDAS_CORE_XMLFDFORMATTARGET_H

#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <nidas/util/IOException.h>

#include <string>

namespace nidas { namespace core {

/**
 * Extension of xercesc::XMLFormatTarget support writing
 * XML to an open device (socket for example).
 */
class XMLFdFormatTarget : public xercesc::XMLFormatTarget {
public:

    /**
     * Constructor.
     * @param n name of device - only used when reporting errors.
     * @param f unix file descriptor of device that is already open.
     */
    XMLFdFormatTarget(const std::string& n, int f);

    /**
     * Destructor.  Does not close file descriptor.
     */
    ~XMLFdFormatTarget();
                                                                                
    void flush() throw(nidas::util::IOException);
                                                                                
    /**
     * Implemention of virtual write method of xercesc::XMLFormatTarget.
     * Does buffered writes to the file descriptor.
     */
    void writeChars(const XMLByte*const toWrite, 
#if XERCES_VERSION_MAJOR < 3
    	const unsigned int count,
#else
    	const XMLSize_t count,
#endif
        xercesc::XMLFormatter *const ) throw(nidas::util::IOException);

private:
    void insureCapacity(unsigned int count) throw(nidas::util::IOException);

    std::string name;
    int fd;
    XMLByte* fDataBuf;
    unsigned int    fIndex;
    unsigned int    fCapacity;
    bool _isSocket;

    /**
     * No copy.
     */
    XMLFdFormatTarget(const XMLFdFormatTarget&);

    /**
     * No assignment.
     */
    XMLFdFormatTarget& operator=(const XMLFdFormatTarget&);
};

}}	// namespace nidas namespace core

#endif
                                                                                
