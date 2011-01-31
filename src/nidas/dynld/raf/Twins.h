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

    void printStatus(std::ostream& ostr) throw();

    void addSampleTag(SampleTag* tag)
            throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

    int getMaxNumChannels() const { return MAX_DMMAT_A2D_CHANNELS; }

    void setA2DParameters(int ichan,int gain,int bipolar)
               throw(nidas::util::InvalidParameterException);

    void getBasicConversion(int ichan,float& intercept, float& slope) const;

private:

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t _badRawSamples;

    int *_ramp;
    void createRamp(); // Uses ramp[512]
    int _waveSize; // defined in XML

    /**
     * Wrapper class for a D2A_Waveform, so that the C struct is automatically
     * freed when a Waveform goes out of scope.
     */ 
    class Waveform {
    public:
        Waveform(int size) 
        {
            _waveform = (struct D2A_Waveform*) malloc(sizeof(struct D2A_Waveform) +
                        sizeof(_waveform->point) * size);
        }

        ~Waveform() { free(_waveform); }
        D2A_Waveform* getPtr() { return _waveform; }
    private:
        struct D2A_Waveform* _waveform;
    };
};

}}	// namespace nidas namespace dynld

#endif
