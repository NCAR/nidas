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

void
BlockStats::
addGoodSample(Sample* samp, long long offset)
{
    // Reset a bad or empty block on a good sample.
    if (!good || !nbytes)
    {
        *this = BlockStats(true, offset);
        start_time = samp->getTimeTag();
        ILOG(("setting good block start to ") << ftime(start_time));
    }
    end_time = samp->getTimeTag();
    ++nsamples;
    last_good_sample_size = samp->getDataByteLength();
    nbytes += samp->getHeaderLength() + last_good_sample_size;
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
    // The block is not empty and has at least one bad byte, but
    // the actual size will not be known until endBadBlock() is called.
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
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(),
    _original(this),_raw(raw)
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
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(),
    _original(this),_raw(raw)
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
    _block(),_badSamples(0),_goodSamples(0),
    _inputHeader(),
    _bsf(x._bsf),
    _original(&x),_raw(x._raw)
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

void SampleInputStream::close() throw(n_u::IOException)
{
    if (_iostream)
    {
        // Finish tallying up the last block.  It would make more sense to
        // compute good and bad blocks per-file, for input streams like
        // FileSets with multiple files.  However, the API for keeping
        // track of when files change makes that difficult.  Likewise, it
        // seems better to close the latest block when EOF is first
        // detected in the stream, but since that exception can be thrown
        // from so many places, this seems the next best option.  So
        // callers which want to log stats on all blocks have to call
        // close() manually first.
        long long offset = _iostream->getNumInputBytes();
        if (_block.nbytes)
        {
            if (!_block.good)
            {
                _block.endBadBlock(0, offset);
            }
            WLOG(("") << _block);
        }
        WLOG(("") << getName() << ": Total " << _badSamples << " bad bytes.");
        WLOG(("") << getName() << ": Total " << _goodSamples
             << " good samples (" << (offset - _badSamples) << " bytes)");
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
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    _inputHeader.read(_iostream);
    _inputHeaderParsed = true;
}

bool SampleInputStream::parseInputHeader() throw(n_u::IOException)
{
    if (!_expectHeader) {
        _inputHeaderParsed = true;
        return true;
    }
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    try {
        _inputHeaderParsed = _inputHeader.parse(_iostream);
    }
    catch(const n_u::ParseException& e) {
        throw n_u::IOException(getName(),"read header",e.what());
    }
    return _inputHeaderParsed;
}

namespace {
    void logBadSampleHeader(const string& name, size_t nbad,
                            long long pos, bool raw,
                            const SampleHeader& header)
    {
        if (raw && header.getType() != CHAR_ST) {
            WLOG(("%s: raw sample not of type char(%d): "
                  "#bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%u",
                  name.c_str(), CHAR_ST, nbad, pos,
                  GET_DSM_ID(header.getId()), GET_SPS_ID(header.getId()),
                  (int)header.getType(), header.getDataByteLength()));
        }
        else {
            WLOG(("%s: bad sample header: "
                  "#bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%u",
                  name.c_str(), nbad, pos,
                  GET_DSM_ID(header.getId()), GET_SPS_ID(header.getId()),
                  (int)header.getType(), header.getDataByteLength()));
        }
    }
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
    if (_expectHeader && _iostream->isNewInput()) _inputHeaderParsed = false;
    
    if (!_inputHeaderParsed && !parseInputHeader()) return true;

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
        if (keepreading)
            len = _iostream->read(_hptr, _headerToRead);
        else
            len = _iostream->readBuf(_hptr, _headerToRead);
        _headerToRead -= len;
        _hptr += len;

        if (keepreading && _expectHeader && _iostream->isNewInput()) {
                _iostream->backup(len);
                readInputHeader();
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

        if (keepreading && _expectHeader && _iostream->isNewInput()) {
            _iostream->backup(len);
            readInputHeader();  // sets _samp to 0
            // abort this sample data and start over
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

#if __BYTE_ORDER == __BIG_ENDIAN
            _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
            _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
            _sheader.setRawId(bswap_32(_sheader.getRawId()));
#endif

            _samp = sampleFromHeader();
            if (!_samp) {
                continue;
            }

            _dataToRead = _samp->getDataByteLength();
            _dptr = (char*) _samp->getVoidDataPtr();
        }

        // We have a good sample in _samp, see if the time is right.
        if (searching && _samp->getTimeTag() >= search_time)
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
        if (searching)
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
    // Set to zero to disable logging of individual bad samples.
    static unsigned int log_count =
        LogScheme::current().getParameterT
        ("sample_input_stream_bad_sample_log_count", 1000);
    Sample* samp = 0;

    // Mark the offset of this sample.
    long long offset = _iostream->getNumInputBytes() - _sheader.getSizeOf();

    // Screen bad headers.
    //
    // @todo: I'm not sure why non-character samples get filtered in raw
    // mode, since maybe they are still valid samples but just the wrong
    // type.  And if they do somehow indicate a bad sample, then why
    // shouldn't filtering have to be turned on to catch them, like all the
    // other validity checks?
    if ((_raw && _sheader.getType() != CHAR_ST) ||
        _bsf.invalidSampleHeader(_sheader))
    {
        samp = 0;
    }
    else
    {
        // getSample can return NULL if type or length are bad
        samp = nidas::core::getSample((sampleType)_sheader.getType(),
                                      _sheader.getDataByteLength());
    }

    if (!samp) {
        // At least by logging some of the bad samples by default, it's
        // more likely someone notices the input stream has bad samples.
        ++_badSamples;
        if (log_count && !((_badSamples-1) % log_count))
        {
            logBadSampleHeader(getName(), _badSamples, offset, _raw, _sheader);
        }
        if (_block.good && _block.nbytes)
        {
            // Log the good block which preceded the start of this bad
            // block.
            WLOG(("") << _block);
            _block.startBadBlock(offset);
        }
        // bad header. Shift left by one byte, read next byte.
        memmove(&_sheader, ((const char *)&_sheader)+1, _sheader.getSizeOf() - 1);
        _headerToRead = 1;
        _hptr--;
    } else {
        // Good sample, fill it in.  The data length was set by getSample()
        // above, so only the timestamp and id are left.
        _goodSamples++;
        samp->setTimeTag(_sheader.getTimeTag());
        samp->setId(_sheader.getId());
        if (!_block.good && _block.nbytes)
        {
            _block.endBadBlock(samp, offset);
            // First good sample after a bad block, so log the bad block.
            WLOG(("") << _block);
        }
        _block.addGoodSample(samp, offset);
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
