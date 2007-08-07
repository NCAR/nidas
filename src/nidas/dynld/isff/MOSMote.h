/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_PROPVANE_H
#define NIDAS_DYNLD_ISFF_PROPVANE_H

#include <nidas/dynld/DSMSerialSensor.h>

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

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
