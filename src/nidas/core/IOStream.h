/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_IOSTREAM_H
#define NIDAS_CORE_IOSTREAM_H

#include <nidas/core/IOChannel.h>

#include <iostream>

namespace nidas { namespace core {

class IOStream;

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

    virtual ~IOStream();

    /**
     * Number of bytes available to be copied from the
     * buffer of IOStream.
     */
    inline size_t available() const
    {
        return head - tail;
    }

    bool isNewFile() const { return newFile; }

    /**
     * Do a IOChannel::read into the internal buffer of IOStream.
     * @return number of bytes read. A return value of zero means an
     *		end-of-file was encountered. If the IOChannel is
     *		a FileSet, then a successive read will either
     *		open and read the next file, or throw EOFException if
     *		there is no next file.
     */
    size_t read() throw(nidas::util::IOException);

    /**
     * If the internal buffer is empty, then do an IOChannel::read
     * into the buffer. Then copy data from the internal buffer
     * to the user buffer.
     * @return number of bytes copied. May be less than the user asked
     *		for. A return value of 0 means the internal buffer
     *		was empty, and an end-of-file was encountered.
     */
    size_t read(void* buf, size_t len) throw(nidas::util::IOException);

    size_t skip(size_t len) throw(nidas::util::IOException);

    size_t backup(size_t len) throw();

    /**
     * Read into the user buffer until a terminating character
     * is found.
     * @return number of bytes read.
     *		A return value of 0 means the internal buffer
     *		was empty, and an end-of-file was encountered.
     */
    size_t readUntil(void* buf, size_t len,char term) throw(nidas::util::IOException);

    /**
     * Outgoing data is buffered, and the buffer is written to the
     * physical device either when the buffer is full, or if this
     * many seconds have elapsed since the last write.
     * This is a useful parameter for real-time applications.
     * @param val Number of microseconds between physical writes.
     *        Default: 250000 microseconds (1/4 sec)
     */
    void setMaxTimeBetweenWrites(int val) { maxUsecs = val; }

    /**
     * Write data.  This supports an atomic write of
     * data from multiple buffers into an output buffer.
     * The write either completely succeeds (all buffers written),
     * or completely fails (no buffers written).
     * This prevents partial data samples from being written,
     * and also reduces the need for copying to a temporary buffer.
     * @param bufs Array of pointers to buffers of data to be written.
     * @param lens Array specifying length of each buffer.
     * @param nbufs Number of buffers, the length of bufs and lens.
     * @return true: all data in bufs was copied to output buffer;
     *    false: no data copied because the buffer was full and the
     *    physical device is bogged down. Typically one must
     *    chuck the data and proceed.
     */
    bool write(const void *const * bufs,const size_t* lens, int nbufs)
  	throw(nidas::util::IOException);

    bool write(const void*buf,size_t len) throw (nidas::util::IOException);

    /**
     * Flush buffer to physical device.
     * This is not done automatically by the destructor - the user
     * must call flush before destroying this IOStream.
     */
    void flush() throw(nidas::util::IOException);

    /**
     * Request that IOChannel object open a new file, with a name
     * based on a time.
     * @param t Time to use when creating file name.
     * @param exact Use exact time when creating file name, else
     *        the time is truncated down to an even time interval.
     * @return Start time of next file.
     */
    dsm_time_t createFile(dsm_time_t t,bool exact) throw(nidas::util::IOException)
    {
	// std::cerr << "IOStream::createFile, doing flush" << std::endl;
	flush();
	return iochannel.createFile(t,exact);
    }

    const std::string& getName() const { return iochannel.getName(); }

    size_t getNBytes() const { return nbytes; }

protected:

    IOChannel& iochannel;

    void reallocateBuffer(size_t len);

private:
    /** data buffer */
    char *buffer;

    /** where we insert bytes into the buffer */
    char* head;

    /** where we remove bytes from the buffer */
    char* tail;

    /**
     * The actual buffer size.
     */
    size_t buflen;

    size_t halflen;

    /**
     * One past end of buffer.
     */
    char* eob;

    /**
     * Maximum number of microseconds between physical writes.
     */
    int maxUsecs;

    /**
     * Time of last physical write.
     */
    dsm_time_t lastWrite;

    /**
     * Was the previous read performed on a newly opened file?
     */
    bool newFile;

    size_t nbytes;

    size_t nEAGAIN;

    /** No copying */
    IOStream(const IOStream&);

    /** No assignment */
    IOStream& operator=(const IOStream&);

};

}}	// namespace nidas namespace core

#endif
