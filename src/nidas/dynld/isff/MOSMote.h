// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
        MOS_TimeSyncer(const MOS_TimeSyncer&);
        MOS_TimeSyncer& operator=(const MOS_TimeSyncer&);
    };

    MOS_TimeSyncer _mosSyncher;
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
