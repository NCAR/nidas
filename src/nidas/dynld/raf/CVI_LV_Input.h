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
 * Adjust the time tag on the samples that come back from LabView
 * by overriding the CharacterSensor::process() method.
 */
class CVI_LV_Input: public CharacterSensor
{

public:

    CVI_LV_Input();

    void open(int flags)
	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    IODevice* buildIODevice() throw(nidas::util::IOException);

    /**
     * Adjust the processing of samples which come back from LabView.
     * The first field in the received sample is a seconds-of-day
     * value that was sent to LabView, indicating when the raw data
     * for the sample was sampled.  LabView then echoes the value
     * back, along with some of the raw data and some derived variables.
     * We use the seconds-of-day value to adjust the timetag on the
     * processed data to match when the raw data was sampled, rather
     * than the time the sample was received from LabView.
     */
    bool process(const Sample * samp,list < const Sample * >&results) throw();

private:

    /**
     * Time at 00:00:00 UTC of day
     */
    dsm_time_t _tt0;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
