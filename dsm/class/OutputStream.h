/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_OUTPUTSTREAM_H
#define DSM_OUTPUTSTREAM_H

#include <Output.h>

namespace dsm {

/**
 * A class for buffering and sending data over a stream.
 * Supports non-blocking atomic writes, so that if the device (e.g. a socket)
 * is not responding, the stream won't back up and cause problems.
 */
class OutputStream {

public:
    /**
     * Create OutputStream.
     * @param output Class implement physical output device - socket, or file.
     * @param buflen length of buffer, in bytes.
     */
    OutputStream(Output& output,int buflen);

    ~OutputStream();

    /**
     * Incoming data is buffered, and the buffer written to the
     * physical device either when the buffer is full, or if this
     * many seconds have elapsed since the last write.
     * This is a useful parameter for real-time applications.
     * @param val Number of milliseconds between physical writes.
     *        Default: 1000 milliseconds.
     */
    void setMaxTimeBetweenWrites(int val) { _maxMsecs = val; }

    /**
     * This is a useful parameter for socket connections.
     * If the network is jambed, this parameter reduces
     * the frequency of attempted writes. If a physical
     * write partially succeeds, then successive writes
     * will be delayed by this amount.
     * @param val Number of milliseconds between physical writes.
     *        Default: 250 milliseconds.
     */
    void setWriteBackoffTime(int val) { _backoffMsecs = val; }

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
     * must call flush before destroying this OutputStream.
     */
    void flush() throw(atdUtil::IOException);

    /**
     * close physical device.
     */
    void close() throw(atdUtil::IOException);

    /**
     * Request that Output object open a new file, with a name
     * based on a time.
     */
    dsm_sys_time_t createFile(dsm_sys_time_t t) throw(atdUtil::IOException)
    {
	flush();
	return output.createFile(t);
    }

protected:

    Output& output;

    /**
     * Actual buffer size. We allocate buffers that are twice
     * as large as the user requested in order to provide some
     * protection against data loss.
     */
    size_t _bufsize;

    /**
     * Requested buffer size, and the maximum size of the
     * physical writes.
     */
    size_t _writelen;

    /**
     * Allocated buffers. We use a double buffering scheme, memcpy-ing
     * to one buffer while the other is being queued to the device.
     */
    char *_bufs[2];

    /**
     * Pointers into _bufs.
     * For input buffer, where to place next data.
     *
     * For output buffer, where to write from next time
     * If bufPtrs[outbuf] < bufs[outbuf] + bufN[outbuf] then
     * we haven't completely written the output buffer.
     */
    char *_bufPtrs[2];

    /** how many bytes in each buffer */
    size_t _bufN[2];

    /** initial index to input buffer */
    int _inbuf;

    /** initial index to output buffer */
    int _outbuf;

    /**
     * Maximum number of milliseconds between physical writes.
     */
    int _maxMsecs;

    /**
     * Back off period in milliseconds if physical device is not
     * being cooperative.
     */
    int _backoffMsecs;

    /**
     * Time of last physical write.
     */
    dsm_sys_time_t _lastWrite;

private:

    /** No copying */
    OutputStream(const OutputStream&);

    /** No assignment */
    OutputStream& operator=(const OutputStream&);

};

}

#endif
