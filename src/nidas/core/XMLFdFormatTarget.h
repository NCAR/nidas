// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
                                                                                
