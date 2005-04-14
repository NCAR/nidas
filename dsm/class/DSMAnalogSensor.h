/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: $

    $LastChangedRevision: $

    $LastChangedBy: $

    $HeadURL: $

 ******************************************************************
*/
#ifndef DSMANALOGSENSOR_H
#define DSMANALOGSENSOR_H

#include <RTL_DSMSensor.h>

#include <vector>
#include <map>

namespace dsm {
/**
 * A sensor connected to the DSM A2D
 */
class DSMAnalogSensor : public RTL_DSMSensor {

public:

    DSMAnalogSensor(const std::string& name);
    ~DSMAnalogSensor();

    void addChannel(int chan, int samplingRate, int gain, int offset);

    const std::vector<int>& getChannels() const { return channels; }

    int getSamplingRate(int chan) { return chanInfo[chan].rate; }

    int getGain(int chan) { return chanInfo[chan].gain; }

    int getOffset(int chan) { return chanInfo[chan].offset; }


    /**
     * open the sensor. This opens the associated RT-Linux FIFOs.
     */
    void open(int flags) throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);
	
    /**
     * Synchronize the A/D's with 1PPS from IRIG/GPS
     */

    void run(int msg) throw(atdUtil::IOException);

protected:

    /* What we need to know about a channel */
    struct chan_info {
        int rate;
	int gain;
	int offset;
    };

    std::vector<int> channels;
    std::map<int,struct chan_info> chanInfo;
};

}

#endif
