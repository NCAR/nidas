/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_FILESETINPUT_H
#define DSM_FILESETINPUT_H

#include <string>

#include <Input.h>
#include <atdUtil/InputFileSet.h>

namespace dsm {

/**
 * Implementation of an InputStream from a atdUtil::InputFileSet
 */
class FileSetInput: public Input, public atdUtil::InputFileSet {

public:
    FileSetInput() {}

    virtual ~FileSetInput() {}

    const std::string& getName() const { return getFileName(); }

    void requestConnection(atdUtil::SocketAccepter* service,int pseudoPort)
    	throw(atdUtil::IOException)
    {
    }

    Input* clone() const { return new FileSetInput(*this); }

    size_t read(void* buf, size_t len) throw(atdUtil::IOException)
    {
        return atdUtil::InputFileSet::read(buf,len);
    }
        
    void close() throw(atdUtil::IOException)
    {
        atdUtil::InputFileSet::closeFile();
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
