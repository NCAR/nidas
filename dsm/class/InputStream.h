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

#include <Input.h>

#include <iostream>

namespace dsm {

/**
 * A base class for buffering data.
 */
class InputStream {

public:
    /**
     * Create InputStream.
     * @param input: reference to an Input object providing the physical IO
     * @param buflen: length of buffer, in bytes.
     */
    InputStream(Input& input,size_t buflen = 8192);

    ~InputStream();

    /**
     * Number of bytes available to read from the internal
     * buffer of InputStream.
     */
    inline size_t available() const
    {
        return tail - head;
    }

    /**
     * Shift data in buffer down, then do a devRead into the
     * internal buffer of InputStream.
     */
    size_t read() throw(atdUtil::IOException);

    /**
     * Copy data from the internal buffer to the user buffer.
     @return number of bytes copied.
     */
    size_t read(void* buf, size_t len) throw();

    void close() throw(atdUtil::IOException);

protected:

    Input& input;

    /**
     * Allocated buffer.
     */
    char* buffer;

    size_t buflen;

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

}

#endif
