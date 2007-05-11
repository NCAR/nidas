/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/PSI9116_Sensor.h $

*/

#ifndef NIDAS_DYNLD_RAF_CVI_LV_INPUT_H
#define NIDAS_DYNLD_RAF_CVI_LV_INPUT_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Support for reading the output from the LabView process on the CVI PC.
 */
class CVI_LV_Input: public CharacterSensor
{

public:

    void open(int flags)
	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    IODevice* buildIODevice() throw(nidas::util::IOException);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
