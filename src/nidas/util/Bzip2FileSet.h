// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/Config.h>

#ifdef HAVE_BZLIB_H

#ifndef NIDAS_UTIL_FILESETBZIP2_H
#define NIDAS_UTIL_FILESETBZIP2_H

#define _FILE_OFFSET_BITS 64

#include <nidas/util/FileSet.h>

#include <bzlib.h>

namespace nidas { namespace util {

/**
 * A nidas::util::FileSet, supporting bzip2 compression and uncompression
 * as files are written or read.
 *
 * Bzip2 vs Gzip compression test on a NIDAS file, ~ 1 Gbyte, ISFS file
 * from AHATS field project: isff_20080721_000000.dat, 987580610 bytes
 * Dell Laptop, Pentium M 2 GHz, single CPU, Fedora 10
 *
 * Default compression level of 9, most compression.
 * time bzip2 isff_20080721_000000.dat
 *   real    5m50.235s
 *   user    5m1.884s
 *   sys     0m3.846s
 * CPU usage: 5 minutes, 5 seconds
 * compressed size: 389251057 bytes.  39.4% of the original file
 *
 * gzip default compression level: 6
 * time gzip isff_20080721_000000.dat
 *   real    2m1.276s
 *   user    1m37.765s
 *   sys     0m2.756s
 * CPU usage: 1 minute, 40 seconds
 * compressed size: 455895904 bytes.  46% of the original file
 *
 * bzip2 compression level 5
 * time bzip2 -5 isff_20080721_000000.dat
 *   real    4m59.822s
 *   user    4m18.155s
 *   sys     0m3.778s
 * CPU usage: 4 minutes, 22 seconds
 * compressed size:  389676037 bytes, 39.4% (almost the same as -9)
 *
 * bzip2 compression level 1
 * time bzip2 -1 isff_20080721_000000.dat
 *   real    4m34.279s
 *   user    4m4.335s
 *   sys     0m3.326s
 * CPU usage: 4 minutes, 8 seconds
 * compressed size:  408404234 bytes, 41.3%
 *
 * bzip2 compression level 1, small
 * time bzip2 -1 -s isff_20080721_000000.dat
 *   real    4m54.530s
 *   user    4m3.999s
 *   sys     0m3.532s
 * CPU usage: 4 minutes, 8 seconds
 * compressed size:  408404234 bytes, 41.3%
 *
 *  Conclusion:
 *  gzip/zlib may be a better choice for compressing files on a CPU constrained
 *  embedded system.  The gzip result was only about 7% larger than bzip2
 *  (a 54% reduction in size versus a 63% reduction in size for bzip2),
 *  but took 40% of the cpu time of bzip2.
 */
class Bzip2FileSet: public FileSet {
public:

    /**
     * constructor
     */
    Bzip2FileSet();

    /**
     * Copy constructor. Only permissable before it is opened.
     */
    Bzip2FileSet(const Bzip2FileSet& x);

    /**
     * Assignment operator. Only permissable before it is opened.
     */
    Bzip2FileSet& operator=(const Bzip2FileSet& x);

    /**
     * Virtual constructor.
     */
    Bzip2FileSet* clone() const;

    ~Bzip2FileSet();

    /**
     * Closes any file currently open.  The base implementation
     * closes the file descriptor.  Subclasses override this method
     * to close alternate resources.
     **/
    void closeFile() throw(IOException);

    /**
     * Open a new file for writing.  The @p filename is the path for the
     * new file as generated from the filename template and a time.  The
     * base implementation calls open64() and sets the file descriptor.
     **/
    void openFileForWriting(const std::string& filename) throw(IOException);

    /**
     * Open the next file to be read.
     **/
    void openNextFile() throw(IOException);


    /**
     * Read from current file.
     */
    size_t read(void* buf, size_t count) throw(IOException);

    /**
     * Write to current file.
     */
    size_t write(const void* buf, size_t count) throw(IOException);

    size_t write(const struct iovec* iov, int iovcnt) throw(IOException);

private:

    FILE* _fp;

    BZFILE* _bzfp;

    int _blockSize100k;

    int _small;

    bool _openedForWriting;

};

}}	// namespace nidas namespace util

#endif
#endif
