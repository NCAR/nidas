// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
    size_t available() const
    {
        return _head - _tail;
    }

    /**
     * Did last read(), or read(buf,len) call result in a new
     * file being opened?
     */
    bool isNewInput() const { return _newInput; }

    /**
     * Do an IOChannel::read into the internal buffer of IOStream.
     * @return number of bytes read.
     * If there is still data in the IOStream buffer, then no physical
     * read will be done, and a length of 0 is returned.
     */
    size_t read() throw(nidas::util::IOException);

    /**
     * Copy available bytes from the internal buffer to buf, returning
     * the number of bytes copied, which may be less then
     * len.  An IOChannel::read() is not done, even if the
     * internal buffer is empty.
     */
    size_t readBuf(void* buf, size_t len) throw()
    {
        size_t l = available();
        if (len > l) len = l;
        if (len == 0) return len;
        memcpy(buf,_tail,len);
        _tail += len;
        addNumInputBytes(len);
        return len;
    }

    /**
     * Read len bytes of data into buf.
     * @return Number of bytes read. This will be less than len
     *     if an end of file is encountered, or if the IOChannel
     *     is configured for non-blocking reads and no data is available.
     * This method may perform 0 or more physical reads of the IOChannel.
     */
    size_t read(void* buf, size_t len) throw(nidas::util::IOException);

    /**
     * If the internal buffer is empty, do an IOChannel::read
     * into the buffer. Then skip over len bytes in the user buffer.
     * @return number of bytes skipped. May be less than the user asked for.
     */
    size_t skip(size_t len) throw(nidas::util::IOException);

    /**
     * Move the read buffer pointer backwards by len number of bytes,
     * so that the next readBuf will return data that was previously read.  
     * This just backs over bytes in the current buffer, and does
     * not reposition the physical device.
     * @return The number of bytes backed over, which may be
     *      less than the number requested if there are fewer
     *      than len number of bytes in the buffer.
     */
    size_t backup(size_t len) throw();

    /**
     * Move the read buffer pointer backwards to the beginning of the buffer.
     */
    size_t backup() throw();

    /**
     * Read into the user buffer until a terminating character
     * is found or len-1 bytes have been read. The buffer is NULL
     * terminated.
     * @return number of bytes read, not including the NULL character.
     *  Therefore the return value will not be more than len-1.
     * This method will do an IOChannel::read() until the
     * termination character is found of the buffer is full.
     * Using this method on a non-blocking device may cause
     * a system lockup.  If an IOChannel::isNewInput is 
     * encountered before the readUntil() is satisfied,
     * then any previous contents of the buffer are discarded
     * and the reading proceeds with the new input.
     * @todo throw an exception if used with a non-blocking physical device.
     */
    size_t readUntil(void* buf, size_t len,char term) throw(nidas::util::IOException);

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
     * @flush: force a write to the physical device.
     * @return true: all data in bufs was copied to output buffer;
     *    false: no data copied because the buffer was full and the
     *    physical device is bogged down. Typically one must
     *    chuck the data and proceed.
     */
    size_t write(const struct iovec* iov, int nbufs,bool flush)
  	throw(nidas::util::IOException);

    size_t write(const void*buf,size_t len,bool flush) throw (nidas::util::IOException);

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
	return _iochannel.createFile(t,exact);
    }

    const std::string& getName() const { return _iochannel.getName(); }

    /**
     * Number of bytes read with this IOStream.
     */
    long long getNumInputBytes() const { return _nbytesIn; }

    void addNumInputBytes(int val) { _nbytesIn += val; }

    /**
     * Total number of bytes written with this IOStream.
     */
    long long getNumOutputBytes() const { return _nbytesOut; }

    void addNumOutputBytes(int val) { _nbytesOut += val; }

protected:

    IOChannel& _iochannel;

    void reallocateBuffer(size_t len);

private:
    /** data buffer */
    char *_buffer;

    /** where we insert bytes into the buffer */
    char* _head;

    /** where we remove bytes from the buffer */
    char* _tail;

    /**
     * The actual buffer size.
     */
    size_t _buflen;

    size_t _halflen;

    /**
     * One past end of buffer.
     */
    char* _eob;

    /**
     * Was the previous read performed on a newly opened file?
     */
    bool _newInput;

    long long _nbytesIn;

    long long _nbytesOut;

    size_t _nEAGAIN;

    /** No copying */
    IOStream(const IOStream&);

    /** No assignment */
    IOStream& operator=(const IOStream&);

};

}}	// namespace nidas namespace core

#endif
