
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_XMLFDFORMATTARGET_H
#define DSM_XMLFDFORMATTARGET_H

#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <atdUtil/IOException.h>

#include <string>

namespace dsm {

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
                                                                                
    void flush() throw(atdUtil::IOException);
                                                                                
    /**
     * Implemention of virtual write method of xercesc::XMLFormatTarget.
     * Does buffered writes to the file descriptor.
     */
    void writeChars(const XMLByte*const toWrite, 
    	const unsigned int count,
        xercesc::XMLFormatter *const ) throw(atdUtil::IOException);

private:
    void insureCapacity(unsigned int count) throw(atdUtil::IOException);

    std::string name;
    int fd;
    XMLByte* fDataBuf;
    unsigned int    fIndex;
    unsigned int    fCapacity;
};

}

#endif
                                                                                
