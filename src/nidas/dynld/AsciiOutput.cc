/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-21 08:31:42 -0700 (Tue, 21 Feb 2006) $

    $LastChangedRevision: 3297 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/src/data_dump.cc $
 ********************************************************************

*/

#include <nidas/dynld/AsciiOutput.h>
#include <nidas/core/DSMTime.h>
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

AsciiOutput::AsciiOutput(IOChannel* ioc):
	SampleOutputBase(ioc),format(HEX),
	headerOut(false)
{
}

/*
 * Copy constructor.
 */
AsciiOutput::AsciiOutput(const AsciiOutput& x):
	SampleOutputBase(x),
	ostr(),
	format(x.format),
	prevTT(),
	headerOut(false)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
AsciiOutput::AsciiOutput(const AsciiOutput& x,IOChannel* ioc):
	SampleOutputBase(x,ioc),
	ostr(),
	format(x.format),
	prevTT(),
	headerOut(false)
{
}

AsciiOutput* AsciiOutput::clone(IOChannel* ioc) const
{
    // invoke copy constructor
    if (!ioc) return new AsciiOutput(*this);
    else return new AsciiOutput(*this,ioc);
}

void AsciiOutput::requestConnection(SampleConnectionRequester* requester)
	throw(n_u::IOException)
{
    if (!getIOChannel()) setIOChannel(new UnixIOChannel("stdout",1));
    SampleOutputBase::requestConnection(requester);
}

void AsciiOutput::connect()
	throw(n_u::IOException)
{
    if (!getIOChannel()) setIOChannel(new UnixIOChannel("stdout",1));
    SampleOutputBase::connect();
}

void AsciiOutput::printHeader() throw(n_u::IOException)
{
    ostr << "|--- date time -------| deltaT   bytes" << endl;
    getIOChannel()->write(ostr.str().c_str(),ostr.str().length());
    ostr.str("");
    ostr.clear();
    headerOut = true;
}

bool AsciiOutput::receive(const Sample* samp) throw()
{
    if (!getIOChannel()) return false;

    dsm_time_t tt = samp->getTimeTag();

    if (tt >= getNextFileTime()) {
	createNextFile(tt);
	headerOut = false;
    }

    if (!headerOut) {
	try {
	    printHeader();
	}
	catch(const n_u::IOException& ioe) {
	    n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: %s",getName().c_str(),ioe.what());
	    disconnect();
	    return false;
	}
    }

    dsm_sample_id_t sampid = samp->getId();

    int ttdiff = 0;
    map<dsm_sample_id_t,dsm_time_t>::iterator pti =
    	prevTT.find(sampid);

    if (pti != prevTT.end()) {
        ttdiff = (tt - pti->second) / USECS_PER_MSEC;
	pti->second = tt;
    }
    else prevTT[sampid] = tt;

    n_u::UTime ut(tt);

    ostr << setw(3) << GET_DSM_ID(sampid) << ',' <<
    	setw(5) << GET_SHORT_ID(sampid) << ' ' <<
	ut.format(true,"%Y %m %d %H:%M:%S.%3f ") <<
	setfill(' ') << setw(4) << ttdiff << ' ' <<
	setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';

    switch (samp->getType()) {
    case FLOAT_ST:
	{
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	ostr << setprecision(4) << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/4; i++)
	    ostr << setw(10) << fp[i] << ' ';
	ostr << endl;
	}
	break;
    case CHAR_ST:
    default:
	switch(format) {
	case ASCII:
	    {
	    string dstr((const char*)samp->getConstVoidDataPtr(),
		    samp->getDataByteLength());
	    ostr << dstr << endl;
	    }
	    break;
	case HEX:
	    {
	    const unsigned char* cp =
		    (const unsigned char*) samp->getConstVoidDataPtr();
	    ostr << setfill('0');
	    for (unsigned int i = 0; i < samp->getDataByteLength(); i++)
		ostr << hex << setw(2) << (unsigned int)cp[i] << dec << ' ';
	    ostr << endl;
	    }
	    break;
	case SIGNED_SHORT:
	    {
	    const short* sp =
		    (const short*) samp->getConstVoidDataPtr();
	    ostr << setfill(' ');
	    for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
		ostr << setw(6) << sp[i] << ' ';
	    ostr << endl;
	    }
	    break;
	case UNSIGNED_SHORT:
	    {
	    const unsigned short* sp =
		    (const unsigned short*) samp->getConstVoidDataPtr();
	    ostr << setfill(' ');
	    for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
		ostr << setw(6) << sp[i] << ' ';
	    ostr << endl;
	    }
	    break;
	case FLOAT:
	    {
	    const float* fp =
		    (const float*) samp->getConstVoidDataPtr();
	    ostr << setprecision(4) << setfill(' ');
	    for (unsigned int i = 0; i < samp->getDataByteLength()/4; i++)
		ostr << setw(10) << fp[i] << ' ';
	    ostr << endl;
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

	    ostr << itt.format(true,"%Y %m %d %H:%M:%S.%6f ") << 
		' ' << setw(2) << setfill('0') << hex << (int)status << dec <<
		'(' << n_r::IRIGSensor::statusString(status) << ')';
	    ostr << endl;
	    }
	    break;
	case DEFAULT:
	    break;
	}
	break;
    }

    try {
	getIOChannel()->write(ostr.str().c_str(),ostr.str().length());
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	"%s: %s",getName().c_str(),ioe.what());
	disconnect();
	return false;
    }
    ostr.str("");
    ostr.clear();
    return true;
}

