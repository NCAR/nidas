/*
 ******************************************************************
    Copyright 2010 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: January 2, 2011 $

    $LastChangedRevision:  $

    $LastChangedBy: ryano $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/Twins.h $

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_TWINS_H
#define NIDAS_DYNLD_TWINS_H

#include <nidas/core/DSMSensor.h>
#include <nidas/dynld/DSC_A2DSensor.h>

#include <nidas/linux/diamond/dmd_mmat.h>
// #include <nidas/linux/filters/short_filters.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * One or more sensors connected to a Diamond Systems Corp A2D.
 */
class Twins : public DSC_A2DSensor {

public:

    Twins();
    ~Twins();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    bool process(const Sample* insamp, std::list<const Sample*>& results) throw();

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

private:

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t _badRawSamples;

    /**
     * Create the D2A waveform
     */
    void createRamp(const struct DMMAT_D2A_Conversion& conv,D2A_WaveformWrapper& wave);

    /**
     * Size of output waveforms, which is also the size of the
     * output samples.
     */
    int _waveSize;

    /**
     * Waveform output rate in Hz: waveforms/sec, also the rate of the output samples.
     */
    float _waveRate;

    /**
     * Channel number, from 0, of output A2D channel.
     */
    int _outputChannel;

};

}}	// namespace nidas namespace dynld

#endif
