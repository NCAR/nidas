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
	SampleOutputBase(),_tt0(0),_tas(floatNAN)
{
}

CVIOutput::CVIOutput(IOChannel* ioc):
	SampleOutputBase(ioc),_tt0(0),_tas(floatNAN)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
CVIOutput::CVIOutput(CVIOutput& x,IOChannel* ioc):
	SampleOutputBase(x,ioc),ostr(),_tt0(0),_tas(floatNAN)
{
    if (DerivedDataReader::getInstance()) 
        DerivedDataReader::getInstance()->addClient(this);
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
    ostr << 
	ut.format(true,"%Y %m %d %H:%M:%S.%3f ") << ' ';
#endif
    ostr << setprecision(7) << double((tt - _tt0) / USECS_PER_MSEC) / MSECS_PER_SEC;

    assert(samp->getType() == FLOAT_ST);

    const SampleT<float>* fsamp = (const SampleT<float>*) samp;
    const float* fp = fsamp->getConstDataPtr();

    ostr << setprecision(6);
    // resampler puts an npts variable at the end. Don't send it
    for (unsigned int i = 0; i < samp->getDataLength()-1; i++) {
        // kludge in true air speed
        if (i == 0) {
            if (isnan(_tas)) ostr << ',' << -99.99;
            else ostr << ',' << _tas;
        }
        else {
            if (isnan(fp[i])) ostr << ',' << -99.99;
            else ostr << ',' << fp[i];
        }
    }
    ostr << '\r' << endl;

    try {
	getIOChannel()->write(ostr.str().c_str(),ostr.str().length());
        // cerr << ostr.str();
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

