/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_INPUTSTREAM_H
#define DSM_INPUTSTREAM_H

#include <atdUtil/Socket.h>
#include <atdUtil/InputFileSet.h>
#include <atdUtil/IOException.h>

#include <dsm_sample.h>

#include <limits.h>

namespace dsm {

/**
 * A virtual base class for reading data.
 */
class InputStream {

public:
    /**
     * Create InputStream.
     * @buflen: length of buffer, in bytes.
     */
    InputStream(size_t buflen);

    virtual ~InputStream();

    /**
     * Number of bytes available to read from the internal buffer of InputStream.
     */
    inline size_t available() const
    {
        return tail - head;
    }

    /**
     * Shift data in buffer down, then do a devRead into the internal buffer of
     * InputStream.
     */
    size_t read() throw(atdUtil::IOException);

    /**
     * Copy data from the internal buffer to the user buffer.
     @return number of bytes copied.
     */
    size_t read(void* buf, size_t len) throw();

protected:

    /**
     * Physical read method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t devRead(void* buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
     * Allocated buffer.
     */
    char* buffer;

    /**
     * End of allocated buffer.
     */
    char* eob;

    /** pointer to next characters available in our buffer */
    char* head;

    /** one past the end of the next characters available in out buffer */
    char* tail;

private:

    /** No copying */
    InputStream(const InputStream&);

    /** No assignment */
    InputStream& operator=(const InputStream&);

};

/**
 * Factory which allows one to create an instance of a class
 * derived from InputStream. Currently supports
 * SocketInputStream and FilesetInputStream.
 */
class InputStreamFactory {
public:
    static InputStream* createInputStream(atdUtil::Socket& sock);
    static InputStream* createInputStream(atdUtil::InputFileSet& fset);
};

}

#endif
