/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_XMLFDINPUTSOURCE_H
#define DSM_XMLFDINPUTSOURCE_H
                                                                                
#include <xercesc/sax/InputSource.hpp>
#include <XMLFdBinInputStream.h>

namespace dsm {

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
}

#endif

