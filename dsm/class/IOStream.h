/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_IOSTREAM_H
#define DSM_IOSTREAM_H

#include <IOChannel.h>

#include <iostream>

namespace dsm {

/**
 * A base class for buffering data.
 */
class IOStream {

public:
    /**
     * Create IOStream.
     * @param input: reference to an Input object providing the physical IO
     * @param buflen: length of buffer, in bytes.
     */
    IOStream(IOChannel& input,size_t buflen = 8192);

    ~IOStream();

    /**
     * Number of bytes available to be copied from the
     * buffer of IOStream.
     */
    inline size_t available() const
    {
        return head - tail;
    }

    /**
     * Do a IOChannel::read into the internal buffer of IOStream.
     */
    size_t read() throw(atdUtil::IOException);

    /**
     * Copy data from the internal buffer to the user buffer.
     @return number of bytes copied.
     */
    size_t read(void* buf, size_t len) throw();

    /**
     * Incoming data is buffered, and the buffer written to the
     * physical device either when the buffer is full, or if this
     * many seconds have elapsed since the last write.
     * This is a useful parameter for real-time applications.
     * @param val Number of milliseconds between physical writes.
     *        Default: 1000 milliseconds.
     */
    void setMaxTimeBetweenWrites(int val) { maxMsecs = val; }

    /**
     * This is a useful parameter for socket connections.
     * If the network is jambed, this parameter reduces
     * the frequency of attempted writes. If a physical
     * write partially succeeds, then successive writes
     * will be delayed by this amount.
     * @param val Number of milliseconds between physical writes.
     *        Default: 250 milliseconds.
     */
    void setWriteBackoffTime(int val) { backoffMsecs = val; }

    /**
     * Write data.  This supports an atomic write of
     * data from multiple buffers into an output buffer for later
     * tranmission. The write either completely
     * succeeds (all buffers written), or completely fails (no buffers
     * written). This prevents partial data samples from being written,
     * and also reduces the need for copying to a temporary buffer.
     * @return true: all data in bufs was received; false: no data
     *         copied because the buffer was full and the
     *         physical device is bogged down. Most processes
     *         will chuck the data and proceed.
     */
    bool write(const void** bufs, size_t* lens, int nbufs)
  	throw(atdUtil::IOException);

    /**
     * Flush buffer to physical device.
     * This is not done automatically by the destructor - the user
     * must call flush before destroying this IOStream.
     */
    void flush() throw(atdUtil::IOException);

    /**
     * Request that IOChannel object open a new file, with a name
     * based on a time.
     */
    dsm_time_t createFile(dsm_time_t t) throw(atdUtil::IOException)
    {
	// std::cerr << "IOStream::createFile, doing flush" << std::endl;
	flush();
	return iochannel.createFile(t);
    }

protected:

    IOChannel& iochannel;

    /** data buffer */
    char *buffer;

    /** where we insert bytes into the buffer */
    char* head;

    /** where we remove bytes from the buffer */
    char* tail;

    /**
     * One half the actual buffer size.
     */
    size_t buflen;

    /**
     * One past end of buffer.
     */
    char* eob;

    /**
     * Maximum number of milliseconds between physical writes.
     */
    int maxMsecs;

    /**
     * Back off period in milliseconds if physical device is not
     * being cooperative.
     */
    int backoffMsecs;

    /**
     * Time of last physical write.
     */
    dsm_time_t lastWrite;

private:

    /** No copying */
    IOStream(const IOStream&);

    /** No assignment */
    IOStream& operator=(const IOStream&);

};

}

#endif
