// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ********************************************************************
 */


#include <nidas/util/Bzip2FileSet.h>

#ifdef HAVE_BZLIB_H

#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>

#include <assert.h>

using namespace nidas::util;
using namespace std;


Bzip2FileSet::Bzip2FileSet() : FileSet(), _fp(0),_bzfp(0),_blockSize100k(1),
    _small(0),_openedForWriting(false)
{
}

/* Copy constructor. */
Bzip2FileSet::Bzip2FileSet(const Bzip2FileSet& x):
    FileSet(x), _fp(0),_bzfp(0),
    _blockSize100k(x._blockSize100k),_small(x._small),
    _openedForWriting(false)
{
}

/* Assignment operator. */
Bzip2FileSet& Bzip2FileSet::operator=(const Bzip2FileSet& rhs)
{
    if (&rhs != this) {
        closeFile();
        (*(FileSet*)this) = rhs;
        _blockSize100k = rhs._blockSize100k;
        _small = rhs._small;
        _openedForWriting = false;
    }
    return *this;
}

Bzip2FileSet* Bzip2FileSet::clone() const
{
    return new Bzip2FileSet(*this);
}


Bzip2FileSet::~Bzip2FileSet()
{
    try {
        closeFile();
    }
    catch(const IOException& e) {}
}

void Bzip2FileSet::openFileForWriting(const std::string& filename) throw(IOException)
{
    int bzerror;
    FileSet::openFileForWriting(filename);
    if ((_fp = ::fdopen(getFd(),"w")) == NULL) {
        _lastErrno = errno; // queried by status method
        closeFile();
        throw IOException(filename,"fdopen",_lastErrno);
    }

    // _blockSize100k is between 1 and 9
    // verbosity: 0 is silent, other values up to 4 give debugging output.
    // workFactor (0-250): lower values reduce the amount of effort bzip2 will expend
    // before resorting to the fallback algorithm when faced with repetitive data.
    // Default is 30, which is selected by a value of 0. Nidas data is not repetitive,
    // so this shouldn't be a critical parameter.
    _bzfp = BZ2_bzWriteOpen(&bzerror,_fp,_blockSize100k,0,0);
    if (!_bzfp) {
        switch(bzerror) {
        case BZ_OK:
            assert(_bzfp);      // strange situation: bzfp==NULL, and bzerror==BZ_OK
            break;  
        case BZ_CONFIG_ERROR:
            _lastErrno = EIO;   // queried by status method
            closeFile();
            throw IOException(filename,"BZ2_bzWriteOpen: bad installation of libbzip2",EIO);
        case BZ_PARAM_ERROR:
            _lastErrno = EIO;   // queried by status method
            closeFile();
            throw IOException(filename,"BZ2_bzWriteOpen: bad value for blockSize100k",EINVAL);
        case BZ_IO_ERROR:
            _lastErrno = errno; // queried by status method
            closeFile();
            throw IOException(filename,"BZ2_bzWriteOpen",errno);
        case BZ_MEM_ERROR:
            _lastErrno = ENOMEM;    // queried by status method
            closeFile();
            throw IOException(filename,"BZ2_bzWriteOpen",ENOMEM);
        }
    }
    _openedForWriting = true;
}

void Bzip2FileSet::openNextFile() throw(IOException)
{
    FileSet::openNextFile();
    if (getFd() == 0) _fp = stdin;  // read from stdin
    else if ((_fp = ::fdopen(getFd(),"r")) == NULL) {
        int ierr = errno;
        closeFile();
        throw IOException(getCurrentName(),"fdopen",ierr);
    }

    int bzerror;
    // _small: if 1 the bzip library will try to decompress using
    // less memory at the expense of speed.
    // unused, nUnused: left over bytes to uncompress
    _bzfp = BZ2_bzReadOpen(&bzerror,_fp,0,_small,NULL,0);
    if (!_bzfp) {
        switch(bzerror) {
        case BZ_OK:
            assert(_bzfp);      // strange situation: bzfp==NULL, and bzerror==BZ_OK
            break;
        case BZ_CONFIG_ERROR:
            closeFile();
            throw IOException(getCurrentName(),"BZ2_bzReadOpen: bad installation of libbzip2",EIO);
        case BZ_PARAM_ERROR:
            closeFile();
            throw IOException(getCurrentName(),"BZ2_bzReadOpen: bad value for parameters",EINVAL);
        case BZ_IO_ERROR:
            closeFile();
            throw IOException(getCurrentName(),"BZ2_bzReadOpen",errno);
        case BZ_MEM_ERROR:
            closeFile();
            throw IOException(getCurrentName(),"BZ2_bzReadOpen",ENOMEM);
        }
    }
    _openedForWriting = false;
}

void Bzip2FileSet::closeFile() throw(IOException)
{
    if (_bzfp != NULL) {
        BZFILE* bzfp = _bzfp;
        _bzfp = 0;
        int bzerror;
        if (_openedForWriting) {
            BZ2_bzWriteClose(&bzerror,bzfp,0,NULL,NULL);
            switch(bzerror) {
            case BZ_OK:
                break;
            case BZ_SEQUENCE_ERROR:
                closeFile();
                throw IOException(getCurrentName(),"BZ2_bzWriteClose: file not opened for writing",EBADF);
            case BZ_IO_ERROR:
                bzerror = errno;
                closeFile();
                throw IOException(getCurrentName(),"BZ2_bzWriteClose",bzerror);
            }
        }
        else {
            BZ2_bzReadClose(&bzerror,bzfp);
            switch(bzerror) {
            case BZ_OK:
                break;
            case BZ_SEQUENCE_ERROR:
                closeFile();
                throw IOException(getCurrentName(),"BZ2_bzReadClose: file not opened for reading",EBADF);
            }
        }
    }
    if (_fp != NULL) {
        FILE* fp = _fp;
        _fp = NULL;
        if (::fclose(fp) == EOF) {                                               
            int ierr = errno;                                                    
            FileSet::closeFile();
            throw IOException(getCurrentName(),"fclose",ierr);                           
        }                     
        _fd = -1;
    }
    FileSet::closeFile();
}

size_t Bzip2FileSet::read(void* buf, size_t count) throw(IOException)
{
    _newFile = false;
    if (getFd() < 0) openNextFile();		// throws EOFException

    int bzerror;
    int res = BZ2_bzRead(&bzerror,_bzfp,buf,count);
    switch(bzerror) {
    case BZ_OK:
        break;
    case BZ_STREAM_END:
        closeFile();	// next read will open next file
        break;
    case BZ_PARAM_ERROR:
        throw IOException(getCurrentName(),"BZ2_bzRead","bad parameters");
    case BZ_SEQUENCE_ERROR:
        throw IOException(getCurrentName(),"BZ2_bzRead","file opened for writing");
    case BZ_IO_ERROR:
        throw IOException(getCurrentName(),"BZ2_bzRead",errno);
    case BZ_UNEXPECTED_EOF:
        WLOG(("%s: ",getCurrentName().c_str()) << ": BZ2_bzRead: unexpected EOF while uncompressing");
        closeFile();	// next read will open next file
        res = 0;
        break;
    case BZ_DATA_ERROR:
    case BZ_DATA_ERROR_MAGIC:
        throw IOException(getCurrentName(),"BZ2_bzRead","bad compressed data");
    case BZ_MEM_ERROR:
        throw IOException(getCurrentName(),"BZ2_bzRead","insufficient memory");
    }
    return res;
}

size_t Bzip2FileSet::write(const void* buf, size_t count) throw(IOException)
{
    int bzerror;
    BZ2_bzWrite(&bzerror,_bzfp,(void*) buf,count);
    switch(bzerror) {
    case BZ_OK:
        break;
    case BZ_PARAM_ERROR:
        throw IOException(getCurrentName(),"BZ2_bzWrite","param error");
    case BZ_SEQUENCE_ERROR:
        throw IOException(getCurrentName(),"BZ2_bzWrite","file not opened for writing");
    case BZ_IO_ERROR:
        _lastErrno = errno; // queried by status method
        throw IOException(getCurrentName(),"BZ2_bzWrite",errno);
    }
    return count;
}

size_t Bzip2FileSet::write(const struct iovec* iov, int iovcnt) throw(IOException)
{
    size_t res = 0;
    for (int i = 0; i < iovcnt; i++) {
        res += write(iov[i].iov_base,iov[i].iov_len);
    }
    return res;
}

#endif
