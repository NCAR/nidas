/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include <nidas/core/SampleInputHeader.h>

#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
const SampleInputHeader::headerField SampleInputHeader::headers[] = {
    { "archive version:",16, &SampleInputHeader::setArchiveVersion,
	    &SampleInputHeader::getArchiveVersion,false },
    { "software version:",17, &SampleInputHeader::setSoftwareVersion,
	    &SampleInputHeader::getSoftwareVersion,false },
    { "project name:",13, &SampleInputHeader::setProjectName,
	    &SampleInputHeader::getProjectName,false },
    { "system name:",12, &SampleInputHeader::setSystemName,
	    &SampleInputHeader::getSystemName,false },
    { "config name:",12, &SampleInputHeader::setConfigName,
	    &SampleInputHeader::getConfigName,false },
    { "config version:",15, &SampleInputHeader::setConfigVersion,
	    &SampleInputHeader::getConfigVersion,false },
    // old
    { "site name:",10, &SampleInputHeader::setSystemName,
	    &SampleInputHeader::getSystemName,true },
    { "observation period name:",24, &SampleInputHeader::setDummyString,
	    &SampleInputHeader::getDummyString,true },
    { "xml name:",9, &SampleInputHeader::setDummyString,
	    &SampleInputHeader::getDummyString,true },
    { "xml version:",12, &SampleInputHeader::setDummyString,
	    &SampleInputHeader::getDummyString,true },

    { "end header\n",11, 0, 0,false },
};

/* static */
const int SampleInputHeader::_ntags = (int)(sizeof(headers)/sizeof(struct headerField));

/* static */
const char* SampleInputHeader::magicStrings[] = {
     "NIDAS (ncar.ucar.edu)\n",
     "NCAR ADS3\n"
};

/* static */
const int SampleInputHeader::_nmagic = (int)(sizeof(magicStrings) / sizeof(magicStrings[0]));

/* static */
const int SampleInputHeader::HEADER_BUF_LEN = 256;

SampleInputHeader::SampleInputHeader():
    _archiveVersion(),_softwareVersion(),_projectName(),_systemName(),
    _configName(),_configVersion(),_dummy(),
    _minMagicLen(INT_MAX), _imagic(-1),
    _endTag(-1),_tagMatch(-1),
    _size(0),
    _buf(0),_headPtr(0),
    _stage(PARSE_START)
{
    for (int i = 0; i < _nmagic; i++) {
        int nc = ::strlen(magicStrings[i]);
        assert(nc < HEADER_BUF_LEN - 1);
	_minMagicLen = std::min(nc,_minMagicLen);
    }
    _buf = new char[HEADER_BUF_LEN];
    _headPtr = _buf;
    for (int i = 0; i < _ntags; i++) {
        assert((int)::strlen(headers[i].tag) == headers[i].taglen);
        assert(headers[i].taglen < HEADER_BUF_LEN - 1);
        // end tag is the one with no set function
        if (!headers[i].setFunc) _endTag = i;
    }
    assert(_endTag >= 0);
}

SampleInputHeader::SampleInputHeader(const SampleInputHeader& x):
    _archiveVersion(x._archiveVersion),
    _softwareVersion(x._softwareVersion),
    _projectName(x._projectName),
    _systemName(x._systemName),
    _configName(x._configName),
    _configVersion(x._configVersion),_dummy(),
    _minMagicLen(x._minMagicLen), _imagic(x._imagic),
    _endTag(x._endTag),_tagMatch(x._tagMatch),
    _size(x._size),
    _buf(0),_headPtr(0),
    _stage(x._stage)
{
    _buf = new char[HEADER_BUF_LEN];
    _headPtr = _buf;
}

SampleInputHeader& SampleInputHeader::operator =(const SampleInputHeader& x)
{
    if (this != &x) {
        _archiveVersion = x._archiveVersion;
        _softwareVersion = x._softwareVersion;
        _projectName = x._projectName;
        _systemName = x._systemName;
        _configName = x._configName;
        _configVersion = x._configVersion;
        _minMagicLen = x._minMagicLen;
        _imagic = x._imagic;
        _endTag = x._endTag;
        _tagMatch = x._tagMatch;
        _size = x._size;
        _stage = x._stage;
        // don't copy contents of user buffer, but set 
        // head pointer to begining.
        _headPtr = _buf;
    }
    return *this;
}

SampleInputHeader::~SampleInputHeader()
{
    delete [] _buf;
}

/*
 * Return: true: finished parsing input header, false: more to parse
 */
bool SampleInputHeader::parse(IOStream* iostream)
	throw(n_u::ParseException)
{
    for (;;) {
        switch (_stage) {
        case PARSE_START:
            _size = 0;
            _stage = PARSE_MAGIC;
            // fallthrough
        case PARSE_MAGIC:
            if (!parseMagic(iostream)) return false;
            // fallthrough
        case PARSE_TAG:
            if (!parseTag(iostream)) return false;
            // don't fall through. The end tag doesn't have a value
            break;
        case PARSE_VALUE:
            if (!parseValue(iostream)) return false;
            break;
        case PARSE_DONE:
            _stage = PARSE_START;
            return true;
        }
    }
    return true;
}

void SampleInputHeader::read(IOStream* iostream)
	throw(n_u::IOException)
{
    try {
        while(!parse(iostream)) {
            iostream->read();
        }
    }
    catch(const n_u::ParseException& e) {
        throw n_u::IOException(iostream->getName(),"read header",e.what());
    }
}

/*
 * Return: true: done parsing magic string, false: not done, need to read more
 */
bool SampleInputHeader::parseMagic(IOStream* iostream)
	throw(n_u::ParseException)
{
    int len;
    int rlen;
    rlen = _minMagicLen - (int)(_headPtr - _buf);
    if (rlen > 0) {
        assert(_headPtr + rlen < _buf + HEADER_BUF_LEN);
        len = iostream->readBuf(_headPtr,rlen);
        _headPtr += len;
        _size += len;
        if (len < rlen) return false;
    }

    for (_imagic = 0; _imagic < _nmagic; _imagic++) {
        if (!::strncmp(_buf,magicStrings[_imagic],_minMagicLen)) {
            // partial match to magic string, read entire string
            int lmagic = (int)::strlen(magicStrings[_imagic]);
            rlen = lmagic - (_headPtr - _buf);
            if (rlen > 0) {
                assert(_headPtr + rlen < _buf + HEADER_BUF_LEN);
                len = iostream->readBuf(_headPtr,rlen);
                _headPtr += len;
                _size += len;
                if (len < rlen) return false;
            }
            if (!::strncmp(_buf,magicStrings[_imagic],lmagic)) {
                rlen = (_headPtr - _buf) - lmagic;
                if (rlen > 0) ::memmove(_buf,_buf + lmagic,rlen);
                _headPtr = _buf + rlen;
                break;
            }
        }
    }
    if (_imagic == _nmagic) {
        iostream->backup();
        _stage = PARSE_START;
        _headPtr = _buf;
        throw n_u::ParseException(string("header does not match \"") + magicStrings[0] + 
            string("\""));
    }
    _stage = PARSE_TAG;
    // cerr << "_imagic=" << _imagic << " _size=" << _size << endl;
    return true;
}

bool SampleInputHeader::parseTag(IOStream* iostream)
	throw(n_u::ParseException)
{
    _tagMatch = -1;
    int ncbuf = _headPtr - _buf;
    for (int ntry=0; _tagMatch < 0; ntry++) {
        if (ncbuf == 0 || ntry > 0) {
            assert(_headPtr + 1 < _buf + HEADER_BUF_LEN);
            if (iostream->readBuf(_headPtr,1) == 0) return false;
            _headPtr++;
            _size++;
            ncbuf++;
        }
        int itag;
        for (itag = 0; itag < _ntags; itag++) {
            if (ncbuf > headers[itag].taglen) continue;
            if (!::strncmp(headers[itag].tag,_buf,ncbuf)) {
                if (ncbuf == headers[itag].taglen) _tagMatch = itag;
                break;
            }
        }
	// no match to any tag
	if (itag == _ntags) {
            // skip leading white space
            if (::isspace(*_buf)) {
                if (ncbuf > 1) ::memmove(_buf,_buf+1,ncbuf-1);
                _headPtr--;
                ncbuf--;
                continue;
            }
            // read a few more characters so we can put out a bit more info
            if (ncbuf < 10) {
                assert(_headPtr + 10 < _buf + HEADER_BUF_LEN);
                int len = iostream->readBuf(_headPtr,20);
                _headPtr += len;
                _size += len;
            }
            ostringstream ost;
            ost << "No match found for header string: \"";
            for (const char* cp = _buf; cp < _headPtr; cp++) {
                if (::isprint(*cp)) ost << *cp;
                else ost << "\\x" << hex << setw(2) <<  (int)(*cp);
            }
            ost << "\"";
            iostream->backup();
            _headPtr = _buf;
            _stage = PARSE_START;
            throw n_u::ParseException(ost.str());
	}
    }
    // complete match to a tag
    _headPtr = _buf;
    if (_tagMatch == _endTag) _stage = PARSE_DONE;
    else _stage = PARSE_VALUE;
    // cerr << "_tagMatch=" << _tagMatch << " _size=" << _size << endl;
    return true;
}

bool SampleInputHeader::parseValue(IOStream* iostream)
	throw(n_u::ParseException)
{
    if (_tagMatch == _endTag) return true;
    assert(_tagMatch >= 0 && _tagMatch < _ntags);
    for(;;) {
        if (_headPtr - _buf >= HEADER_BUF_LEN) {
            iostream->backup();
            _headPtr = _buf;
            _stage = PARSE_START;
            throw n_u::ParseException(
                string("no value found for tag ") + headers[_tagMatch].tag);
        }
        if (iostream->readBuf(_headPtr,1) == 0) return false;
        _size++;
        if (*_headPtr++ == '\n') break;
    }
    _headPtr--; // points to newline

    // skip leading white space.
    const char* cp1;
    for (cp1 = _buf; cp1 < _headPtr && ::isspace(*cp1); cp1++);
    // lop trailing white space.
    const char* cp2;
    for (cp2 = _headPtr; cp2 >= cp1 && ::isspace(*cp2); cp2--);
    string value;
    if (cp2 >= cp1) value = string(cp1,(int)(cp2-cp1)+1);

    // cerr << "value=" << value << " _size=" << _size << endl;
    (this->*headers[_tagMatch].setFunc)(value);
    _headPtr = _buf;
    _stage = PARSE_TAG;
    return true;
}

string SampleInputHeader::toString() const
{
    ostringstream ost;
    ost << magicStrings[0];

    int ntags = sizeof(headers)/sizeof(struct headerField);
    for (int itag = 0; itag < ntags; itag++) {
	if (headers[itag].obsolete) continue;
	const char* str = headers[itag].tag;
        int nc = ::strlen(str);
        if (headers[itag].getFunc) {
            const string& val = (this->*headers[itag].getFunc)();
            ost << str << ' ' << val << '\n';
        }
        else {      // end tag
            int len = ost.str().length();
            int nout = _size;
            if (nout == 0) nout = len + 128;     // add padding
            for ( ; len < nout - nc - 1; len++) ost << ' ';
            for ( ; len < nout - nc; len++) ost << '\n';
            ost << str;
        }
    } 
    return ost.str();
}

size_t SampleInputHeader::write(SampleOutput* output) const
	throw(n_u::IOException)
{
    string hdr = toString();
    return output->write(hdr.c_str(),hdr.length());
}

size_t SampleInputHeader::write(IOStream* output) const
	throw(n_u::IOException)
{
    string hdr = toString();
    return output->write(hdr.c_str(),hdr.length(),true);
}

