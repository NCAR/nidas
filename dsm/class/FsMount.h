/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-10 10:11:54 -0600 (Mon, 10 Oct 2005) $

    $LastChangedRevision: 3042 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/FileSet.h $
 ********************************************************************

*/

#ifndef DSM_FSMOUNT_H
#define DSM_FSMOUNT_H

#include <atdUtil/IOException.h>

#include <DOMable.h>

#include <sys/mount.h>

#include <string>
#include <iostream>

namespace dsm {

/**
 * Filesystem mounter/unmounter.
 */
class FsMount : public DOMable {

public:

    FsMount();

    void setDir(const std::string& val)
    {
        dir = val;
    }

    const std::string& getDir() const { return dir; }

    void setDevice(const std::string& val)
    {
        device = val;
    }

    const std::string& getDevice() const { return device; }

    void setType(const std::string& val)
    {
        type = val;
    }

    const std::string& getType() const { return type; }

    void setOptions(const std::string& val)
    {
        options = val;
    }

    const std::string& getOptions() const { return options; }

    /**
     * Reads /proc/mount to see if getDir() is mounted.
     */
    bool isMounted();

    void mount() throw(atdUtil::IOException);

    void unmount() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);
    
protected:

    std::string dir;

    std::string device;

    std::string type;
 
    std::string options;

};

}

#endif
