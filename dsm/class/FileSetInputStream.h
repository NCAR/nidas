/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_FILESETINPUTSTREAM_H
#define DSM_FILESETINPUTSTREAM_H

#include <string>

#include <InputStream.h>
#include <atdUtil/InputFileSet.h>

namespace dsm {

/**
 * Implementation of an InputStream from a atdUtil::InputFileSet
 */
class FileSetInputStream: public InputStream {

public:
    FileSetInputStream(atdUtil::InputFileSet& fileset) :
  	InputStream(8192),fset(fileset) {
    }

    /**
     * Do the actual hardware write.
     */
    size_t devRead(void* buf, size_t len) throw (atdUtil::IOException)
    {
	return fset.read(buf,len);
    }

protected:
    atdUtil::InputFileSet fset;
};

}

#endif
