/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

