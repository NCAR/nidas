/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_XMLFDBININPUTSTREAM_H
#define DSM_XMLFDBININPUTSTREAM_H
                                                                                
#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <atdUtil/IOException.h>

#include <unistd.h>

#include <iostream>

namespace dsm {

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
    XMLFdBinInputStream(const std::string& n,int f) : name(n),fd(f),curpos(0) {}
    ~XMLFdBinInputStream()
    {
	// std::cerr << "~XMLFdBinInputStream" << std::endl;
    }

    unsigned int curPos() const { return curpos; }

    unsigned int readBytes(XMLByte* toFill,
    	unsigned int maxToRead) throw(atdUtil::IOException)
    {
	// std::cerr << "XMLFdBinInputStream reading " << maxToRead << std::endl;
	unsigned int l = ::read(fd,toFill,maxToRead);
	if (l < 0) throw atdUtil::IOException(name,"read",errno);
	curpos += l;
	// std::cerr << "XMLFdBinInputStream read " << std::string((char*)toFill,0,l < 20 ? l : 20) << std::endl;
	// std::cerr << "XMLFdBinInputStream read " << std::string((char*)toFill,0,l) << std::endl;
	// std::cerr << "XMLFdBinInputStream read " << l << std::endl;
	// toFill[l] = 0;
	// toFill[l+1] = 0;
	return l;
    }

protected:
    
    std::string name;
    
    int fd;
    unsigned int curpos;

};

}

#endif

