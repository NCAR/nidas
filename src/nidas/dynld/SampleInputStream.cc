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
using nidas::util::LogScheme;
using nidas::util::UTime;
using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION(SampleInputStream)

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
}

/*
 * Constructor
 */
SampleInputStream::SampleInputStream(bool raw):
    _iochan(0),_source(raw),_service(0),_iostream(0),_dsm(0),
    _expectHeader(true),_inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _skipSample(false),
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(),
    _original(this),_raw(raw),
    _last_name()
{
}

/*
 * Constructor, with a connected IOChannel.
 */
SampleInputStream::SampleInputStream(IOChannel* iochannel, bool raw):
    _iochan(0),_source(raw),_service(0),_iostream(0),_dsm(0),
    _expectHeader(true),_inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _skipSample(false),
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(),
    _original(this),_raw(raw),
    _last_name()
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
    _samp(0),_dataToRead(0),_dptr(0),
    _skipSample(false),
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(x._bsf),
    _original(&x),_raw(x._raw),
    _last_name()
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

void SampleInputStream::setNonBlocking(bool val) throw(n_u::IOException)
{
    if (_iochan) _iochan->setNonBlocking(val);
}

bool SampleInputStream::isNonBlocking() const throw(n_u::IOException)
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
#ifdef DEBUG
    cerr << getName() << " flush, #clients=" << _source.getClientCount() << endl;
#endif
    // process all samples in buffer
    for (;;) {
        Sample* samp = nextSample();
        if (!samp) break;
        _source.distribute(samp);
    }
}

void SampleInputStream::requestConnection(DSMService* requester)
            throw(n_u::IOException)
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

#ifdef NEEDED

void SampleInputStream::init() throw()
{
#ifdef DEBUG
    cerr << "SampleInputStream::init(), buffer size=" << 
    	_iochan->getBufferSize() << endl;
#endif
    if (!_iostream)
	_iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}
#endif


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

        DLOG(("resetting block stats..."));
        _block = BlockStats();
        _badSamples = 0;
        _goodSamples = 0;
    }
}


void SampleInputStream::close() throw(n_u::IOException)
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

void SampleInputStream::readInputHeader() throw(n_u::IOException)
{
    while (!_inputHeaderParsed)
    {
        _iostream->read();
        // I think this might result in a new input, if the previous input
        // was shorter than the length of a header, in which case the
        // parsing might need to be started over.  But since this has worked
        // so far, nothing is done about it here.
        parseInputHeader();
    }
    DLOG(("input header parsed, offset is now ")
        << _iostream->getNumInputBytes() << " bytes.");
}

bool SampleInputStream::parseInputHeader() throw(n_u::IOException)
{
    // Since this presumably is happening at the start of a new file or
    // new stream, it seems natural to clear out any leftovers from a
    // previous input, even if _expectHeader is false.
    DLOG(("attempting to parse input header"));
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
            throw n_u::IOException(getName(),"read header",e.what());
        }
        ELOG(("skipping header: ") << e.what());
        _inputHeaderParsed = true;
        // I don't think we know whether this will back up to the beginning
        // of the file or not, it only backups up to the beginning of the
        // buffer.
        _iostream->backup();
        ELOG(("backed up iostream to offset ") << _iostream->getNumInputBytes());
    }
    return _inputHeaderParsed;
}

namespace {
    void logBadSampleHeader(const string& name, size_t nbad,
                            long long pos,
                            const SampleHeader& header)
    {
        WLOG(("%s: bad sample header: "
              "#bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%u",
              name.c_str(), nbad, pos,
              GET_DSM_ID(header.getId()), GET_SPS_ID(header.getId()),
              (int)header.getType(), header.getDataByteLength()));
    }
}


void
SampleInputStream::
handleNewInput()
{
    closeBlocks();
    _last_name = getName();
    size_t offset = _iostream->getNumInputBytes();
    _iostream->backup();
    size_t start = _iostream->getNumInputBytes();
    DLOG(("new input detected after reading ")
        << offset << " bytes, backed up to offset " << start);
    _inputHeaderParsed = false;
    readInputHeader();
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
bool SampleInputStream::readSamples() throw(n_u::IOException)
{
    _iostream->read();		// read a buffer's worth

    // no data in buffer after above read, must have been
    // EAGAIN on a non-blocking read
    if (_iostream->available() == 0) return false;

    // first read from a new file
    if (_iostream->isNewInput())
    {
        handleNewInput();
    }

    // process all samples in buffer
    for (;;) {
        Sample* samp = nextSample();
        if (!samp) break;
        _source.distribute(samp);
    }
    return true;
}


bool
SampleInputStream::
readSampleHeader(bool keepreading) throw(n_u::IOException)
{
    while (_headerToRead > 0) {
        size_t len;
        // keepreading means read as much as we need from the stream, even if it
        // means more physical reads of the underlying device.  Otherwise read
        // only what is currently in the buffer.
        if (keepreading)
            len = _iostream->read(_hptr, _headerToRead);
        else
            len = _iostream->readBuf(_hptr, _headerToRead);
        _headerToRead -= len;
        _hptr += len;

        if (_iostream->isNewInput())
        {
            // Always read through the header if any, since the rest of the
            // sample reading code expects to be at a point to read the
            // next sample rather than check if a header needs to be read.
            handleNewInput();
        }
        if (!keepreading && _headerToRead > 0) 
            return false;   // no more data
    }
    return true;
}


bool
SampleInputStream::
readSampleData(bool keepreading) throw(n_u::IOException)
{
    while (_dataToRead > 0) {
        size_t len;
        if (keepreading)
            len = _iostream->read(_dptr, _dataToRead);
        else
            len = _iostream->readBuf(_dptr, _dataToRead);

        if (_iostream->isNewInput())
        {
            handleNewInput();
            return false;
        }

        _dataToRead -= len;
        _dptr += len;

        if (!keepreading && _dataToRead > 0)
            return false;   // no more data
    }
    return true;
}

/*
 * Use readNextSample() to return the next sample from the buffer, if any.
 */
Sample* SampleInputStream::nextSample() throw()
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

/*
 * Try to read a sample, either by reading more from the stream or by
 * reading exclusively from the buffer.  If @p keepreading is false, then
 * only read what's in the buffer and return NULL if there is not a
 * complete sample available.  Otherwise keep reading until a full sample
 * is read.
 */
Sample* SampleInputStream::
nextSample(bool keepreading, bool searching, dsm_time_t search_time)
    throw(n_u::IOException)
{
    Sample* out = 0;
    while (!out) {

        // See if a header needs to be read first.  As soon as the header
        // is read _headerToRead will be zero, and a Sample can be
        // created from the header.
        if (_headerToRead > 0) {

            if (! readSampleHeader(keepreading))
                return 0;

            if (__BYTE_ORDER == __BIG_ENDIAN)
            {
                _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
                _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
                _sheader.setRawId(bswap_32(_sheader.getRawId()));
            }
            _samp = sampleFromHeader();
            if (!_samp) {
                continue;
            }

            // sampleFromHeader() sets _skipSample if there is sample data
            // to read but the sample itself should be ignored.
            _dataToRead = _samp->getDataByteLength();
            _dptr = (char*) _samp->getVoidDataPtr();
        }

        // We have a good sample in _samp, see if the time is right.
        if (!_skipSample &&
            searching && _samp->getTimeTag() >= search_time)
        {
            DLOG(("searching for sample time >= ")
                 << UTime(search_time).format(true)
                 << ", found sample at time: "
                 << UTime(_samp->getTimeTag()).format(true));
            // A sample with the right time has been found, stop here
            // before reading the sample data.
            return 0;
            // We don't want this sample, but drop down to read the data
            // before skipping it.
        }

        if (_samp && !readSampleData(keepreading))
        {
            if (!keepreading)
                return 0;
            // Continue until we finish reading the data.
            continue;
        }

        // If still searching for a sample, free this one and keep going.
        if (_skipSample || searching)
        {
            _samp->freeReference();
            _samp = 0;
        }

        out = _samp;
        _samp = 0;
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
        LogScheme::current().getParameterT
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
            WLOG(("getSample() failed!  ")
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
            logBadSampleHeader(getName(), _badSamples, offset, _sheader);
        }
        if (_block.good && _block.nbytes)
        {
            // Log the good block which preceded the start of this bad
            // block.
            WLOG(("") << _block);
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
        WLOG(("skipping first unfiltered sample at end of bad block: "
              "filepos=%lld,id=(%d,%d),type=%d,len=%u",
              offset,
              GET_DSM_ID(_sheader.getId()), GET_SPS_ID(_sheader.getId()),
              (int)_sheader.getType(), _sheader.getDataByteLength())
              << "," << ftime(_sheader.getTimeTag()));
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
            WLOG(("") << _block);
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
                log->log() << "setting good block start to "
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
Sample* SampleInputStream::readSample() throw(n_u::IOException)
{
    return nextSample(true);
}

/*
 * Search for a sample with timetag >= tt.
 */
void SampleInputStream::search(const UTime& tt) throw(n_u::IOException)
{
    DLOG(("searching for sample time >= ") << tt.format(true));
    nextSample(true, true, tt.toUsecs());
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
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
#ifdef NEEDED
	        istringstream ist(aval);
		float len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SampleInputStream",
			attr.getName(),attr.getValue());
		setSorterLengthMsecs((int)rint(len * MSECS_PER_SEC));
#endif
	    }
	    else if (aname == "heapMax") {
                WLOG(("SampleInputStream: attribute ") << aname << " is deprecated");
#ifdef NEEDED
	        istringstream ist(aval);
		int len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SampleInputStream",
			attr.getName(),attr.getValue());
		setHeapMax(len);
#endif
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

	if (++niochan > 1)
	    throw n_u::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "must have one child element");
    }
    if (!_iochan)
        throw n_u::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
		"input", "must have one child element");
}
