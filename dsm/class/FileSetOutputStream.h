/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_FILESETOUTPUTSTREAM_H
#define DSM_FILESETOUTPUTSTREAM_H

#include <string>

#include <OutputStream.h>
#include <atdUtil/OutputFileSet.h>

namespace dsm {

/**
 * Implementation of an OutputStream to a atdUtil::OutputFileSet
 */
class FileSetOutputStream: public OutputStream {

public:
    FileSetOutputStream(atdUtil::OutputFileSet& fileset) :
  	OutputStream(8192),fset(fileset) {
	setMaxTimeBetweenWrites(10000);		// 10 seconds
	setWriteBackoffTime(0);		// no mercy when writing to disk
    }

    void close() throw(atdUtil::IOException)
    {
        fset.closeFile();
    }

    virtual dsm_sys_time_t createFile(dsm_sys_time_t t) throw(atdUtil::IOException)
    {
	// convert times between time_t and dsm_sys_time_t
	time_t ut = t / 1000;
	return ((dsm_sys_time_t)fset.createFile(ut) * 1000);
    }

    /**
     * Do the actual hardware write.
     */
    size_t devWrite(const void* buf, size_t len) throw (atdUtil::IOException)
    {
      return fset.write(buf,len);
    }

protected:
    atdUtil::OutputFileSet fset;
};

}

#endif
