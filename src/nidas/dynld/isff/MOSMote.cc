/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/MOSMote.h>
#include <nidas/core/Looper.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,MOSMote)

MOSMote::MOSMote():_tsyncPeriodSecs(3600),_ncallBack(0)
{
}

void MOSMote::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    DSMSerialSensor::open(flags);

    const Parameter* tsyncParam = getParameter("tsyncSecs");
    if (tsyncParam && tsyncParam->getType() == Parameter::INT_PARAM &&
    	tsyncParam->getLength() == 1) _tsyncPeriodSecs = (unsigned int)rint(tsyncParam->getNumericValue(0));

    cerr << "tsyncSecs=" << _tsyncPeriodSecs << endl;
    if (_tsyncPeriodSecs > 0)
    	nidas::core::Looper::getInstance()->addClient(this,_tsyncPeriodSecs*MSECS_PER_SEC);

}
void MOSMote::close() throw(nidas::util::IOException)
{
    if (_tsyncPeriodSecs > 0) Looper::getInstance()->removeClient(this);
    DSMSerialSensor::close();
}

void MOSMote::looperNotify() throw()
{
    dsm_time_t tnow = getSystemTime();
    unsigned int msec = (tnow / USECS_PER_MSEC) % (86400 * MSECS_PER_SEC);

    char outmsg[16];
    sprintf(outmsg,"%8d\n",msec);

    cerr << "MOSMote looper callback, msec=" << msec << endl;
    if (!(_ncallBack++ %  30))
	n_u::Logger::getInstance()->log(LOG_INFO,"%s: looperNotify, msec %d",
	    getName().c_str(),msec);

    try {
	if (getWriteFd() >= 0) write(outmsg,strlen(outmsg));
    }
    catch(n_u::IOException & e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
	    getName().c_str(),e.what());
    }
}

bool MOSMote::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{
    unsigned int nc = samp->getDataByteLength();
    if (nc == 0) return false;

    SampleT<char>* nsamp = getSample<char>(nc);

    nsamp->setTimeTag(samp->getTimeTag());
    nsamp->setId(samp->getId());

    const char *cp = (const char*) samp->getConstVoidDataPtr();
    const char *ep = cp + nc;
    char *op = nsamp->getDataPtr();

    for ( ; cp < ep; cp++ ) if (*cp != '\0') *op++ = *cp;
    *op = 0;
    nsamp->setDataLength(op - (char*)nsamp->getDataPtr());

    bool res = DSMSerialSensor::process(nsamp,results);
    nsamp->freeReference();
    return res;
}
