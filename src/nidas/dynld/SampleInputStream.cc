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

#include <nidas/core/Project.h>
#include "SampleInputStream.h"
#include <nidas/core/DSMSensor.h>
#include <nidas/core/DSMService.h>
#include <nidas/core/IOChannel.h>
#include <nidas/core/IOStream.h>
#include <nidas/util/Socket.h>

#include <byteswap.h>

#include <nidas/util/Logger.h>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::Logger;
using nidas::util::UTime;
using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION(SampleInputStream)


/*
 * Log message prefix macro includes the channel name, to help distinguish
 * log messages when multiple SampleInputStream instances are in use,
 * especially when merging.
 */
#define CNAME ("") << getName() << ": "


inline std::string
ftime(dsm_time_t tt)
{
    return UTime(tt).format(true, "%Y-%m-%d,%H:%M:%S");
}

BlockStats::
BlockStats(bool goodblock, size_t startblock):
    start_time(LONG_LONG_MIN),
    end_time(LONG_LONG_MAX),
    good(goodblock),
    nsamples(0),
    block_start(startblock),
    nbytes(0),
    last_good_sample_size(0)
{}

bool
BlockStats::
addGoodSample(Sample* samp, long long offset)
{
    // Reset a bad or empty block on a good sample.
    bool started = false;
    if (!good || !nbytes)
    {
        *this = BlockStats(true, offset);
        start_time = samp->getTimeTag();
        started = true;
    }
    end_time = samp->getTimeTag();
    ++nsamples;
    last_good_sample_size = samp->getDataByteLength();
    nbytes += samp->getHeaderLength() + last_good_sample_size;
    return started;
}


void
BlockStats::
addBadSample(long long offset, unsigned int nbadbytes)
{
    // Reset a good block on a bad sample, but use the end time from
    // the previous block as the start time of this block.
    if (good || !nbytes)
    {
        startBadBlock(offset);
    }
    nbytes += nbadbytes - 1;
}


void
BlockStats::
startBadBlock(long long offset)
{
    dsm_time_t lastgood = end_time;
    *this = BlockStats(false, offset);
    start_time = lastgood;
    // The block is not empty and has at least one bad byte, but
    // the actual size will not be known until endBadBlock() is called.
    nbytes = 1;
}


void
BlockStats::
endBadBlock(Sample* samp, long long offset)
{
    if (samp)
        end_time = samp->getTimeTag();
    nbytes = offset - block_start;
}


std::ostream&
operator<<(std::ostream& out, const BlockStats& bs)
{
    std::ostringstream buf;

    buf << (bs.good ? ".Good" : "..Bad") << " block from ";
    buf << std::dec << std::setw(8) << std::setfill(' ') << bs.block_start
        << " (0x" << std::hex << std::setfill('0') << std::setw(8)
        << bs.block_start << ")";
    buf << " to ";
    buf << std::dec << std::setw(8) << std::setfill(' ') << bs.blockEnd()
        << " (0x" << std::hex << std::setfill('0') << std::setw(8)
        << bs.blockEnd() << ")";
    buf << ", size ";
    buf << std::dec << std::setw(8) << std::setfill(' ') << bs.nbytes
        << " (0x" << std::hex << std::setfill('0') << std::setw(8)
        << bs.nbytes << ")";
    buf << std::dec << std::setw(8) << std::setfill(' ');
    if (bs.good)
    {
        buf << "; number of good samples in block: "
            << bs.nsamples;
        buf << ", from "
            << ftime(bs.start_time) << " to " << ftime(bs.end_time)
            << ", last length: " << bs.last_good_sample_size;
    }
    else
    {
        if (bs.start_time != LONG_LONG_MIN)
            buf << "; last good sample before block: "
                << ftime(bs.start_time) << "; ";
        else
            buf << "no good sample before block; ";
        if (bs.end_time != LONG_LONG_MAX)
            buf << "first good sample after block: "
                << ftime(bs.end_time);
        else
            buf << "no good sample after block.";
    }
    out << buf.str();
    return out;
}

namespace nidas {
    namespace util {
        // Put stream operator for BlockStats in scope where it can be
        // resolved by logging templates in nidas::util.
        using ::operator<<;
    }
    namespace dynld {
        using ::operator<<;
    }
}

/*
 * Constructor
 */
SampleInputStream::SampleInputStream(bool raw):
    _iochan(0),_source(raw),_service(0),_iostream(0),_dsm(0),
    _expectHeader(true),_inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_sampPending(0),_dataToRead(0),_dptr(0),
    _skipSample(false),
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(),
    _original(this),_raw(raw),
    _last_name(),
    _eofx("", ""),
    _ateof(false)
{
}

/*
 * Constructor, with a connected IOChannel.
 */
SampleInputStream::SampleInputStream(IOChannel* iochannel, bool raw):
    _iochan(0),_source(raw),_service(0),_iostream(0),_dsm(0),
    _expectHeader(true),_inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_sampPending(0),_dataToRead(0),_dptr(0),
    _skipSample(false),
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(),
    _original(this),_raw(raw),
    _last_name(),
    _eofx("", ""),
    _ateof(false)
{
    setIOChannel(iochannel);
    _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}

/*
 * Copy constructor, with a new, connected IOChannel.
 */
SampleInputStream::SampleInputStream(SampleInputStream& x,
    IOChannel* iochannel):
    _iochan(0),_source(x._source),_service(x._service),_iostream(0),
    _dsm(x._dsm),
    _expectHeader(x._expectHeader),
    _inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_sampPending(0),_dataToRead(0),_dptr(0),
    _skipSample(false),
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(x._bsf),
    _original(&x),_raw(x._raw),
    _last_name(),
    _eofx("", ""),
    _ateof(false)
{
    setIOChannel(iochannel);
    _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}

/*
 * Clone myself, with a new IOChannel.
 */
SampleInputStream* SampleInputStream::clone(IOChannel* iochannel)
{
    return new SampleInputStream(*this,iochannel);
}

SampleInputStream::~SampleInputStream()
{
    close();
    if (_samp)
        _samp->freeReference();
    if (_sampPending)
        _sampPending->freeReference();
    delete _iochan;
}

void SampleInputStream::setIOChannel(IOChannel* val)
{
    if (val != _iochan) {
        if (_iochan) _iochan->close();
    delete _iochan;
    _iochan = val;
    }
    if (_iochan) {
        setExpectHeader(_iochan->writeNidasHeader());
        n_u::Inet4Address remoteAddr =
            _iochan->getConnectionInfo().getRemoteSocketAddress().getInet4Address();
        if (remoteAddr != n_u::Inet4Address()) {
            _dsm = Project::getInstance()->findDSM(remoteAddr);
        }
    }
}

void SampleInputStream::setNonBlocking(bool val)
{
    if (_iochan) _iochan->setNonBlocking(val);
}

bool SampleInputStream::isNonBlocking() const
{
    if (_iochan) return _iochan->isNonBlocking();
    return false;
}

string SampleInputStream::getName() const {
    if (_iochan) return string("SampleInputStream: ") + _iochan->getName();
    return string("SampleInputStream");
}

int SampleInputStream::getFd() const 
{
    if (_iochan) return _iochan->getFd();
    return -1;
}

void SampleInputStream::flush() throw()
{
    VLOG(CNAME << "flush, #clients="
        << _source.getClientCount());
    // process all samples in buffer
    for (;;) {
        Sample* samp = nextSample();
        if (!samp) break;
        _source.distribute(samp);
    }
}

void SampleInputStream::requestConnection(DSMService* requester)
{
    _service = requester;
    _iochan->requestConnection(this);
}

SampleInput* SampleInputStream::connected(IOChannel* ioc) throw()
{
    if (_iochan && _iochan != ioc) {
        if (_service) {
            SampleInputStream* newist = clone(ioc);
            _service->connect(newist);
            return newist;
        }
        else {
            setIOChannel(ioc);
            delete _iostream;
            _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
        }
    }
    else {
        setIOChannel(ioc);
        delete _iostream;
        _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
        if (_service) _service->connect(this);
    }
    return this;
}


void SampleInputStream::closeBlocks()
{
    // Need two log contexts here, one for warnings when filtering
    // is active and one for debug when there are no bad samples.
    static LogContext wlog(LOG_WARNING);
    static LogContext dlog(LOG_DEBUG);
    LogContext* log =
        (_bsf.filterBadSamples() || _badSamples != 0) ? &wlog : &dlog;

    // Tally up the blocks.  We cannot rely on the iostream to give a
    // reliable end of the current block, because it may have already been
    // reset to a new file.
    size_t length = _block.block_start + _block.nbytes;
    if (_block.nbytes)
    {
        log->log() << _block;
    }
    // It's possible this is the start of the first file, in which case
    // there is nothing to report.
    if (_badSamples || _goodSamples)
    {
        log->log() << _last_name << ": Total " << _badSamples << " bad bytes.";
        log->log() << _last_name << ": Total " << _goodSamples
                << " good samples (" << (length - _badSamples) << " bytes)";

        DLOG(CNAME << "resetting block stats...");
        _block = BlockStats();
        _badSamples = 0;
        _goodSamples = 0;
    }
}


void SampleInputStream::close()
{
    closeBlocks();
    if (_iostream)
    {
        delete _iostream;
        _iostream = 0;
    }
    if (_iochan)
        _iochan->close();
}

const DSMConfig* SampleInputStream::getDSMConfig() const
{
    return _dsm;
}

void SampleInputStream::readInputHeader()
{
    while (!_inputHeaderParsed)
    {
        // There shouldn't be any sample pending here, so it's safe to throw
        // EOF if EOF is hit while reading the header.
        _iostream->read();
        // I think this might result in a new input, if the previous input
        // was shorter than the length of a header, in which case the
        // parsing might need to be started over.  But since this has worked
        // so far, nothing is done about it here.

        // Since this should be the first read on a stream, especially the
        // first file in a FileSet stream, this seems like a good place to
        // cache the name of the current file, so it is available after
        // isNewInput() is detected.
        _last_name = getName();
        DLOG(CNAME << "set _last_name: " << _last_name);
        parseInputHeader();
    }
    DLOG(CNAME << "input header parsed, offset is now "
         << _iostream->getNumInputBytes() << " bytes.");
}

bool SampleInputStream::parseInputHeader()
{
    // Since this presumably is happening at the start of a new file or
    // new stream, it seems natural to clear out any leftovers from a
    // previous input, even if _expectHeader is false.
    DLOG(CNAME << "attempting to parse input header");
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    if (!_expectHeader) {
        _inputHeaderParsed = true;
        return true;
    }
    try {
        _inputHeaderParsed = _inputHeader.parse(_iostream);
    }
    catch(const n_u::ParseException& e) {
        // SampleInputHeader::parse() will throw an exception if the header
        // is read but does not parse, ie, because the magic string is not
        // found.  Skip the header and consider it parsed if that is enabled.
        if (!_bsf.skipNidasHeader())
        {
            throw n_u::IOException(getName(), "read header", e.what());
        }
        PLOG(CNAME << "skipping header: " << e.what());
        _inputHeaderParsed = true;
        // I don't think we know whether this will back up to the beginning
        // of the file or not, it only backups up to the beginning of the
        // buffer.
        _iostream->backup();
        PLOG(CNAME << "backed up iostream to offset "
             << _iostream->getNumInputBytes());
    }
    return _inputHeaderParsed;
}

namespace nidas {
    namespace core {

        std::ostream&
        operator<<(std::ostream& out, const nidas::core::SampleHeader& header)
        {
            out << "id=(" << GET_DSM_ID(header.getId()) << ","
                << GET_SPS_ID(header.getId()) << ");"
                << "type=" << (int)header.getType() << ";"
                << "len=" << header.getDataByteLength() << ";"
                << "time=" << ftime(header.getTimeTag());
            return out;
        }

        std::ostream&
        operator<<(std::ostream& out, const nidas::core::Sample& samp)
        {
            out << *(nidas::core::SampleHeader*)samp.getHeaderPtr();
            return out;
        }
    }
}


/**
 * Whenever we are at the start of a new input, we need to start over with
 * parsing the input header.  This method sets up the right state to start
 * over on a new file.
 **/
void
SampleInputStream::
handleNewInput()
{
    VLOG(CNAME << "entering handleNewInput()...");
    closeBlocks();
    size_t offset = _iostream->getNumInputBytes();
    _iostream->backup();
    size_t start = _iostream->getNumInputBytes();
    DLOG(CNAME << "new input detected after reading "
         << offset << " bytes, backed up to offset " << start);
    _inputHeaderParsed = false;
}


/*
 * Read a buffer of data and process all samples in the buffer.
 * This is typically used when a select has determined that there
 * is data available on our file descriptor. Process all available
 * data from the InputStream and distribute() samples to the receive()
 * method of my SampleClients and to the receive() method of
 * DSMSenors.  This will perform only one physical
 * read of the underlying device.
 */
bool SampleInputStream::readSamples()
{
    // Use read() to fill the buffer, handle new input, and record if EOF is
    // hit.  Then call into nextSample() in case there is a sample pending
    // which needs to be returned before throwing EOF.
    ReadResult rr = read(true, 0, 0);
    VLOG(CNAME << "readSamples() read result: " << "len=" << rr.len
         << ",eof=" << rr.eof << ",newinput=" << rr.newinput);

#ifdef notdef
    // I'm not sure if this is needed.  nextSample() will only
    // use what's available in the buffer, as well as handling
    // other conditions like EOF and pending samples, so it seems
    // clearer to go directly to calling nextSample().

    // no data in buffer after above read, must have been
    // EAGAIN on a non-blocking read
    if (_iostream->available() == 0) return false;
#endif

    // Process all samples in buffer.  Return false if there aren't any.  We
    // have to check for EOF here, because it is not allowed to be thrown
    // from nextSample().
    Sample* samp = nextSample();
    if (!samp && !_ateof)
    {
        return false;
    }
    while (samp)
    {
        _source.distribute(samp);
        samp = nextSample();
    }
    if (_ateof)
        handleEOF(true);
    return true;
}


void
SampleInputStream::
checkUnexpectedEOF()
{
    if (0 < _headerToRead && _headerToRead < _sheader.getSizeOf())
    {
        // Part of a header was read, but not all of it.
        WLOG(CNAME << "unexpected EOF on " << _last_name
        << ": needed " << _headerToRead << " more bytes "
        << "for sample header length " << _sheader.getSizeOf());
    }
    else if (_dataToRead > 0)
    {
        WLOG(CNAME << "unexpected EOF on " << _last_name
        << ": expected " << _dataToRead << " more bytes "
        << "for sample data length " << _samp->getDataByteLength());
    }
}


/**
 * All reads of the iostream go through here.  There are three kinds of
 * reads needed:
 *
 *  - Read to fill the iostream buffer but not a local block. (ptr==0 and
 *    keepreading=true)
 *  - Read from the iostream buffer into a local block (keepreading==false).
 *  - Read from the iostream to fill a local block, filling the buffer as
 *    needed (keepreading=true)
 *
 * In all these cases, we need to catch if we hit the end of a file to know
 * if the file ended unexpectedly.  Also, if we hit the end of a file and a
 * sample is still pending (because filtering is enabled and a sample is not
 * good until it passes the filter and is succeeded by eof or another good
 * sample), then we also need to return that sample.  This method records
 * the eof exception but does not pass it on, waiting instead for it to be
 * thrown after any pending sample has been returned by nextSample().
 * Callers can test ReadResult members for which cases have occurred.
 **/
SampleInputStream::ReadResult
SampleInputStream::
read(bool keepreading, char* ptr, size_t lentoread)
{
    ReadResult rr;
    // keepreading means read as much as we need from the stream, even if it
    // means more physical reads of the underlying device.  Otherwise read
    // only what is currently in the buffer.
    VLOG(CNAME << "vvvvv entering SampleInputStream::read"
         << "(keepreading=" << keepreading
         << ", ptr=" << bool(ptr)
         << ", lentoread=" << lentoread << ")");
    if (keepreading)
    {
        try {
            if (ptr)
            {
                rr.len = _iostream->read(ptr, lentoread);
                VLOG(CNAME << "read(ptr, " << lentoread
                     << ") returned " << rr.len << " bytes read, "
                     << _iostream->available()
                     << " bytes available in buffer.");
            }
            else
            {
                rr.len = _iostream->read();
                VLOG(CNAME << "buffer read() returned "
                     << rr.len << " bytes read, "
                     << _iostream->available()
                     << " bytes available in buffer.");
            }
        }
        catch (nidas::util::EOFException& eof)
        {
            DLOG(CNAME << "EOF caught in SampleInputStream::read()");
            _eofx = eof;
            _ateof = true;
            rr.eof = true;
        }
    }
    else if (ptr)
    {
        rr.len = _iostream->readBuf(ptr, lentoread);
        VLOG(CNAME << "readBuf() returned "
                   << rr.len << " bytes read, "
                   << _iostream->available()
                   << " bytes available in buffer.");
    }
    rr.newinput = _iostream->isNewInput();

    if (rr.newinput || rr.eof)
    {
        checkUnexpectedEOF();
    }
    if (rr.newinput)
    {
        // Always read through the input header if any, since the rest
        // of the sample reading code expects to be at a point to read
        // the next sample rather than check if a header needs to be
        // read.
        handleNewInput();
    }
    VLOG(CNAME << "^^^^^ read(keepreading=" << keepreading
         << ",lentoread=" << lentoread << ") returning read result: "
         << "len=" << rr.len << ",eof=" << rr.eof
         << ",newinput=" << rr.newinput);
    return rr;
}


/**
 * Read a block into memory, updating the given block pointer and length
 * counter accordingly.  If @p keepreading is false, only read into the
 * block what is available from the iostream buffer.  Return the ReadResult
 * of the last read.  Always return if the current input ends or eof is
 * reached.
 **/
SampleInputStream::ReadResult
SampleInputStream::
readBlock(bool keepreading, char* &ptr, size_t& lentoread)
{
    // In order to catch an unexpected EOF, I think we have to account for
    // two scenarios.  One is that one file ends early but there are more
    // files to read, which we have to detect with isNewInput().  The other
    // is that the last file reaches EOF and throws the EOF exception.
    ReadResult rr;
    do
    {
        rr = read(keepreading, ptr, lentoread);
        lentoread -= rr.len;
        ptr += rr.len;
    }
    while (keepreading && lentoread > 0 && !rr.eof && !rr.newinput);
    return rr;
}


/*
 * Use nextSample(keepreading=false) to return the next sample from the
 * buffer, if any.
 */
Sample* SampleInputStream::nextSample()
{
    try {
        return nextSample(false);
    }
    catch (n_u::IOException& ioe)
    {
        // this should not happen when keepreading is passed as false.
    }
    return 0;
}


/**
 * When EOF has been caught, then either we need to return the last pending
 * sample, or else we need to throw the exception.  HOWEVER, because
 * nextSample() is not allowed to throw an IOException when keepreading is
 * false, since in general it does not make sense to trigger EOF when
 * nothing is being read, this handler only throws the exception when
 * keepreading is true.
 **/
Sample*
SampleInputStream::
handleEOF(bool keepreading)
{
    if (_sampPending)
    {
        DLOG(CNAME << "reached eof, returning pending sample");
        Sample* out = _sampPending;
        _sampPending = 0;
        return out;
    }
    if (keepreading)
    {
        DLOG(CNAME << "handleEOF(): no sample pending, keepreading is true, "
             "raising EOFException");
        throw _eofx;
    }
    else
    {
        DLOG(CNAME << "handleEOF(): no sample pending, "
             "but keepreading is false");
    }
    return 0;
}


/*
 * Try to read a sample, either by reading more from the stream or by
 * reading exclusively from the buffer.  If @p keepreading is false, then
 * only read what's in the buffer and return NULL if there is not a
 * complete sample available.  Otherwise keep reading until a full sample
 * is read.
 */
Sample* SampleInputStream::
nextSample(bool keepreading, bool searching, dsm_time_t search_time)
{
    Sample* out = 0;

    // This saves the state of the most recent read, so whenever eof or
    // newinput occur the loop can be restarted and the event can be
    // handled.
    ReadResult rr;

    while (!out)
    {
        // Check if EOF has been hit and there is still a sample pending.
        if (_ateof)
        {
            return handleEOF(keepreading);
        }

        if (rr.newinput && _sampPending)
        {
            // Hit end of a file, so we can send any currently pending
            // sample.  Next call to this method will continue in the new
            // input where we left off.  Note this happens whether
            // keepreading is true or not.
            DLOG(("switched to new input, returning pending sample"));
            out = _sampPending;
            _sampPending = 0;
            return out;
        }

        // If last time through we hit new input, then there may be an input
        // header to read first.  This forces a read of the iostream, even
        // if keepreading is false.
        if (!_inputHeaderParsed)
        {
            readInputHeader();
        }

        // See if a sample header needs to be read.  As soon as the header
        // is read _headerToRead will be zero, and a Sample can be created
        // from the header.
        if (_headerToRead > 0)
        {
            rr = readBlock(keepreading, _hptr, _headerToRead);
            if (rr.eof || rr.newinput)
            {
                // Start over.
                DLOG(CNAME << "restarting from sample header read "
                     << "to read input header");
                continue;
            }

            // We didn't get all of a header yet, keepreading must have been
            // false.
            if (_headerToRead > 0)
            {
                VLOG(CNAME << "nextSample(keepreading=" << keepreading << ") "
                     << "returning 0 because _headerToRead=" << _headerToRead);
                return 0;
            }

            if (__BYTE_ORDER == __BIG_ENDIAN)
            {
                _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
                _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
                _sheader.setRawId(bswap_32(_sheader.getRawId()));
            }
            _samp = sampleFromHeader();
            if (!_samp)
            {
                // The header was corrupt somehow, meaning we have hit
                // corrupt data and any pending sample needs to be dropped.
                if (_sampPending)
                {
                    WLOG(CNAME << "dropping unfiltered sample preceding a bad block: "
                         << *_sampPending);
                    _sampPending->freeReference();
                    _sampPending = 0;
                }
                continue;
            }

            // sampleFromHeader() sets _skipSample if there is sample data
            // to read but the sample itself should be ignored.
            _dataToRead = _samp->getDataByteLength();
            _dptr = (char*) _samp->getVoidDataPtr();
        }

        // If a good sample is pending, then now we can pass on that pending
        // sample as the next good sample, since it was succeeded by a good
        // header.  The next entry to this function will continue with
        // handling the current header.
        if (_sampPending)
        {
            out = _sampPending;
            _sampPending = 0;
            return out;
        }

        // We have a good sample in _samp, see if the time is right.
        if (!_skipSample &&
            searching && _samp->getTimeTag() >= search_time)
        {
            DLOG(CNAME << "searching for sample time >= "
                 << UTime(search_time).format(true)
                 << ", found sample at time: "
                 << UTime(_samp->getTimeTag()).format(true));
            // A sample with the right time has been found, stop here
            // before reading the sample data.
            return 0;
            // We don't want this sample, but drop down to read the data
            // before skipping it.
        }

        if (_samp)
        {
            rr = readBlock(keepreading, _dptr, _dataToRead);
            if (rr.eof || rr.newinput)
            {
                // The current sample could not be read completely
                // before hitting the end of a file.  Start over.
                continue;
            }

            // If there is still data to read for the sample, but the
            // current file has not eneded yet, then keepreading must be
            // false and this should return no sample.
            if (_dataToRead > 0)
            {
                VLOG(CNAME << "nextSample(keepreading=" << keepreading << ") "
                     << "returning 0 with _dataToRead=" << _dataToRead);
                return 0;
            }
        }

        // We now have a good (unfiltered) sample with complete data.
        // Either it needs to be skipped because it succeeds a bad block, or
        // it needs to be held back in case it precedes a bad block.
        //
        // If still searching for a sample, free this one and keep going.
        // Searching presents a real problem, because it expects to return
        // when a good header is read but before the data are read, so it is
        // not possible to see if the sample is followed by a bad block
        // before passing it on as valid.  So as it stands now, when
        // searching for a particular time, it's possible to get a sample
        // which precedes a bad block and might itself have corrupt data.
        if (_skipSample || searching)
        {
            _samp->freeReference();
            _samp = 0;
        }

        // Hold the current sample as pending.  It will be returned above if
        // it is followed with a good header.
        if (_bsf.filterBadSamples())
        {
            _sampPending = _samp;
            _samp = 0;
        }
        else
        {
            out = _samp;
            _samp = 0;
        }

        // next read is the next header
        _headerToRead = _sheader.getSizeOf();
        _hptr = (char*)&_sheader;
    }
    return out;
}


Sample*
SampleInputStream::
sampleFromHeader() throw()
{
    // Default to zero to disable logging of individual bad samples.
    // Logging blocks could be overwhelming too, but seems more likely to
    // be useful.
    static unsigned int log_count =
        Logger::getScheme().getParameterT
        ("sample_input_stream_bad_sample_log_count", 0);
    Sample* samp = 0;

    // Mark the offset of this sample.
    long long offset = _iostream->getNumInputBytes() - _sheader.getSizeOf();

    // Screen bad headers when enabled.
    //
    // At one point this check required samples in raw streams to have
    // sample type CHAR_ST, so some kind of sample filtering would happen
    // even if it wasn't enabled.  Since then the bad sample filtering has
    // become more capable, so this code just relies on that.  If filtering
    // is not enabled, then getSample() below might still catch an invalid
    // sample and go nuts reporting it.  getSample() always checks for valid
    // sample type, reasonable length, and length that is a multiple of the
    // size of the sample type.  The log meesages will be the user's cue to
    // turn on filtering and tune it as needed, but that may still need
    // adjustment.  It also seems cleaner if SampleInputStream does not have
    // to know if the samples it is reading are "raw" or not.  If the
    // CHAR_ST check is needed for a particular sample stream, it can be
    // enabled in the BadSampleFilter with the 'raw' rule.
    if (_bsf.filterBadSamples() && _bsf.invalidSampleHeader(_sheader))
    {
        samp = 0;
    }
    else
    {
        // getSample can return NULL if type or length are bad
        samp = nidas::core::getSample((sampleType)_sheader.getType(),
                                      _sheader.getDataByteLength());
        if (!samp)
        {
            WLOG(CNAME << "getSample() failed!  "
                 << "type=" << (int)_sheader.getType()
                 << "; len=" << _sheader.getDataByteLength());
        }
    }

    if (!samp) {
        // At least by logging some of the bad samples by default, it's
        // more likely someone notices the input stream has bad samples.
        _skipSample = false;
        ++_badSamples;
        if (log_count && !((_badSamples-1) % log_count))
        {
            WLOG(CNAME << "bad sample header: "
                 << "#bad=" << _badSamples << ","
                 << "filepos=" << offset << "," << _sheader);
        }
        if (_block.good && _block.nbytes)
        {
            // Log the good block which preceded the start of this bad
            // block.
            WLOG(CNAME << _block);
            _block.startBadBlock(offset);
        }
        else
        {
            // Append to the existing bad block.  We need to do this now
            // while we know the current stream offset, since in a
            // multi-file stream the offset resets to zero before we find
            // out the current file has been closed.
            _block.endBadBlock(0, offset);
        }
        // bad header. Shift left by one byte, read next byte.
        memmove(&_sheader, ((const char *)&_sheader)+1, _sheader.getSizeOf() - 1);
        _headerToRead = 1;
        _hptr--;
    }
    else if (!_block.good && _block.nbytes && !_skipSample)
    {
        // This is the first good sample immediately after a bad block.
        // However, since we still can't be sure the time is not corrupted
        // at the beginning of the sample, skip this sample.
        _skipSample = true;
        WLOG(CNAME << "skipping first unfiltered sample at end of bad block: "
             "filepos=" << offset << _sheader);
    }
    else
    {
        // Good sample, fill it in.  The data length was set by getSample()
        // above, so only the timestamp and id are left.
        _skipSample = false;
        _goodSamples++;
        samp->setTimeTag(_sheader.getTimeTag());
        samp->setId(_sheader.getId());
        if (!_block.good && _block.nbytes)
        {
            _block.endBadBlock(samp, offset);
            // First good sample after a bad block, so log the bad block.
            WLOG(CNAME << _block);
        }
        if (_block.addGoodSample(samp, offset))
        {
            // This is not generally useful information, so limit it to debug
            // unless filtering is on or there have been bad blocks.
            static LogContext dlog(LOG_DEBUG);
            static LogContext wlog(LOG_WARNING);
            LogContext* log =
                (_bsf.filterBadSamples() || _badSamples) ? &wlog : &dlog;
            if (log->active())
            {
                log->log() << getName() << ": setting good block start to "
                           << ftime(_block.start_time);
            }
        }
    }
    return samp;
}


/*
 * Read the next full sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
Sample* SampleInputStream::readSample()
{
    return nextSample(true);
}

/*
 * Search for a sample with timetag >= tt.
 */
void SampleInputStream::search(const UTime& tt)
{
    DLOG(CNAME << "searching for sample time >= " << tt.format(true));
    nextSample(true, true, tt.toUsecs());
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
{
    XDOMElement xnode(node);

    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            // const std::string& aval = attr.getValue();
            // Sample sorter length in seconds
            if (aname == "sorterLength") {
                WLOG(("SampleInputStream: attribute ") << aname << " is deprecated");
            }
            else if (aname == "heapMax") {
                WLOG(("SampleInputStream: attribute ") << aname << " is deprecated");
            }
        }
    }

    // process <socket>, <fileset> child elements (should only be one)

    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

        IOChannel* iochan = IOChannel::createIOChannel((const xercesc::DOMElement*)child);
        iochan->fromDOMElement((xercesc::DOMElement*)child);
        setIOChannel(iochan);

        if (++niochan > 1) {
            throw n_u::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
                "input", "must have one child element");
        }
    }
    if (!_iochan) {
        throw n_u::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
        "input", "must have one child element");
    }
}
