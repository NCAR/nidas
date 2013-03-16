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

#include <nidas/dynld/AsciiOutput.h>
#include <nidas/core/UnixIOChannel.h>
#include <nidas/dynld/raf/IRIGSensor.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iomanip>

using namespace std;
using namespace nidas::dynld;
using namespace nidas::core;

namespace n_u = nidas::util;
namespace n_r = nidas::dynld::raf;

NIDAS_CREATOR_FUNCTION(AsciiOutput)

AsciiOutput::AsciiOutput():
    SampleOutputBase(),_ostr(),
    _format(HEX),_prevTT(),_headerOut(false)
{
}

AsciiOutput::AsciiOutput(IOChannel* ioc,SampleConnectionRequester* rqstr):
    SampleOutputBase(ioc,rqstr),_ostr(),
    _format(HEX),_prevTT(),_headerOut(false)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
AsciiOutput::AsciiOutput(AsciiOutput& x,IOChannel* ioc):
    SampleOutputBase(x,ioc),
    _ostr(),_format(x._format),
    _prevTT(),_headerOut(false)
{
}

AsciiOutput* AsciiOutput::clone(IOChannel* ioc)
{
    // invoke copy constructor
    return new AsciiOutput(*this,ioc);
}

void AsciiOutput::requestConnection(SampleConnectionRequester* requester)
    throw()
{
    if (!getIOChannel()) setIOChannel(new UnixIOChannel("stdout",1));
    SampleOutputBase::requestConnection(requester);
}

void AsciiOutput::connect(SampleSource* source)
	throw(n_u::IOException)
{
    if (!getIOChannel()) setIOChannel(new UnixIOChannel("stdout",1));
    source->addSampleClient(this);
}

void AsciiOutput::printHeader() throw(n_u::IOException)
{
    _ostr << "|- id --| |--- date time -------| deltaT    bytes" << endl;
    getIOChannel()->write(_ostr.str().c_str(),_ostr.str().length());
    _ostr.str("");
    _ostr.clear();
    _headerOut = true;
}

bool AsciiOutput::receive(const Sample* samp) throw()
{
    if (!getIOChannel()) return false;

    dsm_time_t tt = samp->getTimeTag();

    if (tt >= getNextFileTime()) {
	createNextFile(tt);
	_headerOut = false;
    }

    if (!_headerOut) {
	try {
	    printHeader();
	}
	catch(const n_u::IOException& ioe) {
	    n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: %s",getName().c_str(),ioe.what());
            // this disconnect may schedule this object to be deleted
            // in another thread, so don't do anything after the
            // disconnect except return;
	    disconnect();
	    return false;
	}
    }

    dsm_sample_id_t sampid = samp->getId();

    int ttdiff = 0;
    map<dsm_sample_id_t,dsm_time_t>::iterator pti =
    	_prevTT.find(sampid);

    if (pti != _prevTT.end()) {
        ttdiff = (double)(tt - pti->second) / USECS_PER_SEC;
	pti->second = tt;
    }
    else _prevTT[sampid] = tt;

    n_u::UTime ut(tt);

    _ostr << setw(3) << GET_DSM_ID(sampid) << ',' <<
    	setw(5) << GET_SHORT_ID(sampid) << ' ' <<
	ut.format(true,"%Y %m %d %H:%M:%S.%3f ") <<
	setfill(' ') << setprecision(3) << setw(5) << ttdiff << ' ' <<
	setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';

    switch (samp->getType()) {
    case FLOAT_ST:
    case DOUBLE_ST:
	{
	_ostr << setprecision(7) << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataLength(); i++)
	    _ostr << setw(10) << samp->getDataValue(i) << ' ';
	_ostr << endl;
	}
	break;
    case CHAR_ST:
    default:
	switch(_format) {
	case ASCII:
	    {
	    string dstr((const char*)samp->getConstVoidDataPtr(),
		    samp->getDataByteLength());
	    _ostr << dstr << endl;
	    }
	    break;
	case HEX:
	    {
	    const unsigned char* cp =
		    (const unsigned char*) samp->getConstVoidDataPtr();
	    _ostr << setfill('0');
	    for (unsigned int i = 0; i < samp->getDataByteLength(); i++)
		_ostr << hex << setw(2) << (unsigned int)cp[i] << dec << ' ';
	    _ostr << endl;
	    }
	    break;
	case SIGNED_SHORT:
	    {
	    const short* sp =
		    (const short*) samp->getConstVoidDataPtr();
	    _ostr << setfill(' ');
	    for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
		_ostr << setw(6) << sp[i] << ' ';
	    _ostr << endl;
	    }
	    break;
	case UNSIGNED_SHORT:
	    {
	    const unsigned short* sp =
		    (const unsigned short*) samp->getConstVoidDataPtr();
	    _ostr << setfill(' ');
	    for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
		_ostr << setw(6) << sp[i] << ' ';
	    _ostr << endl;
	    }
	    break;
	case FLOAT:
	    {
	    const float* fp =
		    (const float*) samp->getConstVoidDataPtr();
	    _ostr << setprecision(6) << setfill(' ');
	    for (unsigned int i = 0; i < samp->getDataByteLength()/4; i++)
		_ostr << setw(10) << fp[i] << ' ';
	    _ostr << endl;
	    }
	    break;
	case IRIG:
	    {
	    const unsigned char* dp =
	    	(const unsigned char*) samp->getConstVoidDataPtr();
	    struct timeval tv;
	    memcpy(&tv,dp,sizeof(tv));
	    dp += sizeof(tv);
	    unsigned char status = *dp;

	    n_u::UTime itt((dsm_time_t) tv.tv_sec * USECS_PER_SEC
	    	+ tv.tv_usec);

	    _ostr << itt.format(true,"%Y %m %d %H:%M:%S.%6f ") << 
		' ' << setw(2) << setfill('0') << hex << (int)status << dec <<
		'(' << n_r::IRIGSensor::statusString(status) << ')';
	    _ostr << endl;
	    }
	    break;
	case DEFAULT:
	    break;
	}
	break;
    }

    try {
	getIOChannel()->write(_ostr.str().c_str(),_ostr.str().length());
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	"%s: %s",getName().c_str(),ioe.what());
        // this disconnect may schedule this object to be deleted
        // in another thread, so don't do anything after the
        // disconnect except return;
	disconnect();
	return false;
    }
    _ostr.str("");
    _ostr.clear();
    return true;
}

