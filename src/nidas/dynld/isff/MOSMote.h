/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_MOSMOTE_H
#define NIDAS_DYNLD_ISFF_MOSMOTE_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/LooperClient.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * A DSMSerialSensor for support of an early, buggy version of
 * a Mantis OS Mote, which insert null ('\x00') characters
 * in the middle of their output, after about every 64 characters.
 * The MOSMote::process method simply creates another sample
 * without the nulls and passes it to the DSMSerialSensor process method.
 */
class MOSMote: public nidas::dynld::DSMSerialSensor
{
public:

    MOSMote();

    void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    void close() throw(nidas::util::IOException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

private:
    unsigned int _tsyncPeriodSecs;

    unsigned int _ncallBack;

    class MOS_TimeSyncer: public nidas::core::LooperClient
    {
    public:
        MOS_TimeSyncer(MOSMote* mote): _mote(mote) {}

        /**
         * LooperClient virtual method.
         */
        void looperNotify() throw();
    private:
        MOSMote* _mote;
    };

    MOS_TimeSyncer _mosSyncher;
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
