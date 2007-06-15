/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleInputHeader.h>

#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
SampleInputHeader::headerField SampleInputHeader::headers[] = {
    { "archive version:", &SampleInputHeader::setArchiveVersion,
	    &SampleInputHeader::getArchiveVersion,false },
    { "software version:", &SampleInputHeader::setSoftwareVersion,
	    &SampleInputHeader::getSoftwareVersion,false },
    { "project name:", &SampleInputHeader::setProjectName,
	    &SampleInputHeader::getProjectName,false },
    { "system name:", &SampleInputHeader::setSystemName,
	    &SampleInputHeader::getSystemName,false },
    { "config name:", &SampleInputHeader::setConfigName,
	    &SampleInputHeader::getConfigName,false },
    { "config version:", &SampleInputHeader::setConfigVersion,
	    &SampleInputHeader::getConfigVersion,false },
    // old
    { "site name:", &SampleInputHeader::setSystemName,
	    &SampleInputHeader::getSystemName,true },
    { "observation period name:", &SampleInputHeader::setDummyString,
	    &SampleInputHeader::getDummyString,true },
    { "xml name:", &SampleInputHeader::setDummyString,
	    &SampleInputHeader::getDummyString,true },
    { "xml version:", &SampleInputHeader::setDummyString,
	    &SampleInputHeader::getDummyString,true },

    { "end header\n", 0, 0,false },
};

const char* SampleInputHeader::magicStrings[] = {
     "NIDAS (ncar.ucar.edu)\n",
     "NCAR ADS3\n"
};

void SampleInputHeader::check(IOStream* iostream)
	throw(n_u::IOException)
{
    char buf[512];

    int nmagic = sizeof(magicStrings) / sizeof(magicStrings[0]);
    // cerr << "nmagic=" << nmagic << endl;

    _size = 0;
    size_t minl = 99999999;
    int imagic;
    for (imagic = 0; imagic < nmagic; imagic++)
	minl = std::min(::strlen(magicStrings[imagic]),minl);

    if (iostream->read(buf,minl) != minl)
    	throw n_u::IOException(iostream->getName(),"open",
	    string("header does not match \"") + magicStrings[0] + 
	    string("\""));

    _size += minl;

    for (imagic = 0; imagic < nmagic; imagic++) {
	if (!::strncmp(buf,magicStrings[imagic],minl)) {
            // partial match to magic string, read entire string
	    size_t lmagic = ::strlen(magicStrings[imagic]);
	    size_t nread = lmagic - _size;
	    if (nread > 0 && iostream->read(buf+_size,nread) != nread)
		throw n_u::IOException(iostream->getName(),"read",
                    "short header (does not contain magic string)");
            _size += nread;
	    if (!::strncmp(buf,magicStrings[imagic],lmagic)) break;
	}
    }
    if (imagic == nmagic)
    	throw n_u::IOException(iostream->getName(),"open",
	    string("header does not match \"") + magicStrings[0] + 
	    string("\""));

    int ntags = sizeof(headers)/sizeof(struct headerField);

    unsigned int ic = 0;
    for ( ;; ) {
        if (iostream->read(buf+ic,1) == 0) break;
	ic++;
        _size++;
	int itag;
	for (itag = 0; itag < ntags; itag++)
	    if (!strncmp(headers[itag].tag,buf,ic)) break;

	// no match to any tag, keep looking
	if (itag == ntags) {
	    if (!isspace(buf[ic-1])) {
		buf[ic] = '\0';
	        n_u::Logger::getInstance()->log(LOG_WARNING,
		    "No match for header string: \"%s\"",buf);
	    }
	    ic = 0;
	    continue;
	}

	if (ic == strlen(headers[itag].tag)) {	// complete tag match
	    ic = 0;
	    if (!headers[itag].setFunc) break;		// endheader tag
	    for(;;) {
		if (iostream->read(buf+ic,1) == 0) return; // EOF
                _size++;
		if (buf[ic] == '\n') break;
		if (++ic == sizeof(buf)) break;
	    }
	    // no newline found for this tag value
	    if (ic == sizeof(buf) || buf[ic] != '\n') 
		throw n_u::IOException(iostream->getName(),"open",
		    string("no value found for tag ") + headers[itag].tag);

	    unsigned int is = 0;
	    // skip leading white space.
	    for (is = 0; is < ic && isspace(buf[is]); is++);
	    // lop trailing white space.
	    for ( ; ic > is && isspace(buf[ic-1]); ic--);
	    string value = string(buf+is,ic-is);

	    // cerr << headers[itag].tag << ' ' << value << endl;
	    (this->*headers[itag].setFunc)(value);

	    ic = 0;
	}
    } 
    _size -= ic;
    iostream->backup(ic);
}

size_t SampleInputHeader::write(SampleOutput* output)
	throw(n_u::IOException)
{
    int nout = 0;
    nout += output->write(magicStrings[0],::strlen(magicStrings[0]));

    int ntags = sizeof(headers)/sizeof(struct headerField);

    int itag;
    for (itag = 0; itag < ntags; itag++) {
	if (headers[itag].obsolete) continue;
	const char* str = headers[itag].tag;
        if (headers[itag].getFunc) { 
            nout += output->write(str,::strlen(str));
	    nout += output->write(" ",1);
	    str = (this->*headers[itag].getFunc)().c_str();
	    nout += output->write(str,::strlen(str));
	    nout += output->write("\n",1);
	}
        else {      // end tag
            nout += ::strlen(str);
            if (_size == 0) _size = nout + 128;     // add padding
            for ( ; nout < _size - 1; ) nout += output->write(" ",1);
            if (nout < _size) nout += output->write("\n",1);
            output->write(str,::strlen(str));
        }
    } 
    return nout;
}

size_t SampleInputHeader::write(IOStream* output)
	throw(n_u::IOException)
{
    int nout = 0;
    nout += output->write(magicStrings[0],::strlen(magicStrings[0]));

    int ntags = sizeof(headers)/sizeof(struct headerField);

    int itag;
    for (itag = 0; itag < ntags; itag++) {
	if (headers[itag].obsolete) continue;
	const char* str = headers[itag].tag;
        if (headers[itag].getFunc) { 
            nout += output->write(str,::strlen(str));
	    nout += output->write(" ",1);
	    str = (this->*headers[itag].getFunc)().c_str();
	    nout += output->write(str,::strlen(str));
	    nout += output->write("\n",1);
	}
        else {      // end tag
            nout += ::strlen(str);
            if (_size == 0) _size = nout + 128;     // add padding
            for ( ; nout < _size - 1; ) nout += output->write(" ",1);
            if (nout < _size) nout += output->write("\n",1);
            output->write(str,::strlen(str));
        }
    } 
    return nout;
}

