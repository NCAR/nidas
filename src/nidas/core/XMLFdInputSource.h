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

#ifndef NIDAS_CORE_XMLFDINPUTSOURCE_H
#define NIDAS_CORE_XMLFDINPUTSOURCE_H
                                                                                
#include <xercesc/sax/InputSource.hpp>
#include <nidas/core/XMLFdBinInputStream.h>

namespace nidas { namespace core {

/**
 * Implemenation of xercesc::InputSource, that returns an XMLFdBinInputStream
 * for reading from a Unix file descriptor - i.e. an opened socket
 * for example.
 */
class XMLFdInputSource: public xercesc::InputSource {
public:

    /**
     * Constructor.
     * @param n name of device - only used when reporting errors.
     * @param f unix file descriptor of device that is already open.
     */
    XMLFdInputSource(const std::string& n,int f) : name(n),fd(f) {}
    ~XMLFdInputSource() {
    	// std::cerr << "~XMLFdInputSource" << std::endl;
    }

    /**
     * Create an instance of a BinInputStream.  Pointer becomes
     * the parser's property.
     */
    xercesc::BinInputStream* makeStream() const {
        return new XMLFdBinInputStream(name,fd);
    }

protected:
    std::string name;
    int fd;

};

}}	// namespace nidas namespace core

#endif

