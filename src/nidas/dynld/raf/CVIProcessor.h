/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/CVIProcessor.h $
 ********************************************************************
*/

#ifndef __nidas_dynld_raf_CVIProcessor_h
#define __nidas_dynld_raf_CVIProcessor_h

#include <memory>

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/dynld/ViperDIO.h>
#include <nidas/dynld/DSC_AnalogOut.h>
#include <nidas/core/SampleAverager.h>
#include <nidas/core/NearestResamplerAtRate.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Processor to support Counter-flow Virtual Impactor.
 */
class CVIProcessor: public SampleIOProcessor, public SampleClient
{
public:

    CVIProcessor();

    /**
     * Copy constructor.
     */
    CVIProcessor(const CVIProcessor&);

    ~CVIProcessor();

    CVIProcessor* clone() const;

    /**
     * No need to clone.
     */
    bool cloneOnConnection() const { return false; }

    void addSampleTag(SampleTag* tag)
	    throw(nidas::util::InvalidParameterException);
    /**
     * Do common operations necessary when a input has connected:
     * 1. Copy the DSMConfig information from the input to the
     *    disconnected outputs.
     * 2. Request connections for all disconnected outputs.
     *
     * connect() methods in subclasses should do whatever
     * initialization necessary before invoking this
     * CVIProcessor::connect().
     */
    void connect(SampleInput*) throw(nidas::util::IOException);

    /**
     * Disconnect a SampleInput from this CVIProcessor.
     * Right now just does a flush() of all connected outputs.
     */
    void disconnect(SampleInput*) throw(nidas::util::IOException);

    /**
     * Do common operations necessary when a output has connected:
     * 1. do: output->init().
     * 2. add output to a list of connected outputs.
     */
    void connected(SampleOutput* orig,SampleOutput* output) throw();

    /**
     * Do common operations necessary when a output has disconnected:
     * 1. do: output->close().
     * 2. remove output from a list of connected outputs.
     */
    void disconnected(SampleOutput* output) throw();

    void setD2ADeviceName(const std::string& val)
    {
        _d2aDeviceName = val;
    }

    const std::string& getD2ADeviceName() const
    {
        return _d2aDeviceName;
    }

    void setDigIODeviceName(const std::string& val)
    {
        _digioDeviceName = val;
    }

    const std::string& getDigIODeviceName()
    {
        return _digioDeviceName;
    }

    bool receive(const Sample*s) throw();

    void fromDOMElement(const xercesc::DOMElement* node)
        throw(nidas::util::InvalidParameterException);

protected:

    void attachLVInput(const SampleTag* tag)
        throw(nidas::util::IOException);

private:

    std::string _d2aDeviceName;

    std::string _digioDeviceName;

    std::vector<const Variable*> _variables;

    std::vector<bool> _varMatched;

    SampleSorter _sorter;

    SampleAverager _averager;

    SampleInputWrapper _avgWrapper;

    NearestResamplerAtRate* _resampler;

    std::auto_ptr<SampleInputWrapper> _rsmplrWrapper;

    float _rate;

    dsm_sample_id_t _lvSampleId;

    DSMSensor* _lvSensor;

    DSC_AnalogOut _aout;

    ViperDIO _dout;

    unsigned int _numD2A;

    unsigned int _numDigout;

    float _vouts[5];

    int _douts[4];

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
