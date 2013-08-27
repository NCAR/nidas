// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/CVIOutput.cc $
 ********************************************************************

*/

#include <nidas/dynld/raf/CVIOutput.h>
#include <nidas/core/UnixIOChannel.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iomanip>

using namespace std;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace nidas::core;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CVIOutput)

CVIOutput::CVIOutput():
	SampleOutputBase(),_ostr(),
        _variables(),_tt0(0),_tas(floatNAN)
{
}

CVIOutput::CVIOutput(IOChannel* ioc,SampleConnectionRequester* rqstr):
	SampleOutputBase(ioc,rqstr),_ostr(),
        _variables(),_tt0(0),_tas(floatNAN)
{
    if (DerivedDataReader::getInstance()) 
        DerivedDataReader::getInstance()->addClient(this);
    setName("raf.CVIOutput: " + getIOChannel()->getName());
}

/*
 * Copy constructor, with a new IOChannel.
 */
CVIOutput::CVIOutput(CVIOutput& x,IOChannel* ioc):
	SampleOutputBase(x,ioc),_ostr(),
        _variables(x._variables), _tt0(0),_tas(floatNAN)
{
    if (DerivedDataReader::getInstance()) 
        DerivedDataReader::getInstance()->addClient(this);
    setName("raf.CVIOutput: " + getIOChannel()->getName());
}

CVIOutput::~CVIOutput()
{
    if (DerivedDataReader::getInstance()) 
        DerivedDataReader::getInstance()->removeClient(this);
}

CVIOutput* CVIOutput::clone(IOChannel* ioc)
{
    // invoke copy constructor
    return new CVIOutput(*this,ioc);
}

void CVIOutput::setIOChannel(IOChannel* val)
{
    if (DerivedDataReader::getInstance()) 
        DerivedDataReader::getInstance()->addClient(this);
    SampleOutputBase::setIOChannel(val);
    setName("raf.CVIOutput: " + getIOChannel()->getName());
}

void CVIOutput::addRequestedSampleTag(SampleTag* tag)
    throw(n_u::InvalidParameterException)
{

    VariableIterator vi = tag->getVariableIterator();
    for ( ; vi.hasNext(); ) {
        const Variable* var = vi.next();
        _variables.push_back(var);
    }
    SampleOutputBase::addRequestedSampleTag(tag);
}

void CVIOutput::requestConnection(SampleConnectionRequester* requester) throw()
{
    if (!getIOChannel()) setIOChannel(new UnixIOChannel("stdout",1));
    if (DerivedDataReader::getInstance()) 
        DerivedDataReader::getInstance()->addClient(this);
    SampleOutputBase::requestConnection(requester);
}

void CVIOutput::derivedDataNotify(const DerivedDataReader * rdr)
    throw()
{
    _tas = rdr->getTrueAirspeed();
    // cerr << "got true air speed=" << _tas << endl;
}

bool CVIOutput::receive(const Sample* samp) throw()
{

    /*
        dsmtime   fractional seconds since midnite no rollover,
        cvtas,    true air speed from airplane
        cnt1,   counts
        cvf1,cvfxo,cvfx1,cxfx2,cvfx3,cvfx4,cxfx5,cvfx6,cvfx7,cxfx8,cvpcn,cvtt,cvtp,cvts,cvtcn,cvtai,
        H2OR,pdlR,ttdlR,TDLsignal,TDLline,TDLlaser,TDLzero,
        opcd,opcc,opcco,opcdf
    */

    if (!getIOChannel()) return false;

    dsm_time_t tt = samp->getTimeTag();

    if (tt >= getNextFileTime()) {
	createNextFile(tt);
    }

    // midnight
    if (_tt0 == 0) _tt0 = tt - (tt % USECS_PER_DAY);

#ifdef FULL_TIME
    n_u::UTime ut(tt);
    _ostr << 
	ut.format(true,"%Y %m %d %H:%M:%S.%3f ") << ' ';
#endif
    _ostr << setprecision(7) << double((tt - _tt0) / USECS_PER_MSEC) / MSECS_PER_SEC;

    assert(samp->getType() == FLOAT_ST);

    const SampleT<float>* fsamp = (const SampleT<float>*) samp;
    const float* fp = fsamp->getConstDataPtr();

    _ostr << setprecision(6);
    for (unsigned int i = 0; i < samp->getDataLength(); i++) {
        // kludge in true air speed
        if (i == 0) {
            if (isnan(_tas)) _ostr << ',' << -99.99;
            else _ostr << ',' << _tas;
        }
        else {
            if (isnan(fp[i])) _ostr << ',' << -99.99;
            else _ostr << ',' << fp[i];
        }
    }
    _ostr << '\r' << endl;

    try {
	getIOChannel()->write(_ostr.str().c_str(),_ostr.str().length());
        // cerr << _ostr.str();
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	"%s: %s",getName().c_str(),ioe.what());
        // this disconnect will schedule this object to be deleted
        // in another thread, so don't do anything after the
        // disconnect except return;
	disconnect();
	return false;
    }
    _ostr.str("");
    _ostr.clear();
    return true;
}

