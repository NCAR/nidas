/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_FILESETOUTPUT_H
#define DSM_FILESETOUTPUT_H

#include <string>

#include <Output.h>
#include <atdUtil/OutputFileSet.h>

namespace dsm {

/**
 * Implementation of an OutputStream from a atdUtil::OutputFileSet
 */
class FileSetOutput: public Output, public atdUtil::OutputFileSet {

public:
    FileSetOutput() {}

    virtual ~FileSetOutput() {}

    const std::string& getName() const { return getFileName(); }

    void requestConnection(atdUtil::SocketAccepter* service,int pseudoPort)
    	throw(atdUtil::IOException)
    {
    }

    Output* clone() const { return new FileSetOutput(*this); }

    size_t write(const void* buf, size_t len) throw(atdUtil::IOException)
    {
        return atdUtil::OutputFileSet::write(buf,len);
    }
        
    void close() throw(atdUtil::IOException)
    {
        atdUtil::OutputFileSet::closeFile();
    }
        
    int getFd() const { return -1; }
        
    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);
    
protected:
};

}

#endif
