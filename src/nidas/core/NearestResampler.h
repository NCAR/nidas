// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_NEARESTRESAMPLER_H
#define NIDAS_CORE_NEARESTRESAMPLER_H

#include <nidas/core/Resampler.h>
#include <nidas/core/SampleTag.h>

#include <vector>

namespace nidas { namespace core {

/**
 * A simple, nearest-point resampler, for generating merged
 * samples from variables from one or more sample sources.
 * The first variable added to NearestResample with the
 * addVariable() method becomes the "master" variable.
 * The time tags of the master variable become the output
 * sample time tags, and values of other variables are merged
 * into the output sample by associating those values with
 * the nearest time tag to the master times.
 *
 * The only requirement is that the samples which are fed
 * to the receive() method should be sorted in time. It they
 * aren't sorted some data will be lost.
 * NearestResampler does not need to know sampling
 * rates, and the sampling rates of the input variables, including
 * the master variable, may vary.
 */
class NearestResampler : public Resampler {
public:

    /**
     * Constructor.
     */
    NearestResampler(const std::vector<const Variable*>& vars,bool nansVariable=true);

    NearestResampler(const std::vector<Variable*>& vars,bool nansVariable=true);

    ~NearestResampler();

    SampleSource* getRawSampleSource() { return 0; }

    SampleSource* getProcessedSampleSource() { return &_source; }

    /**
     * Get the SampleTag of my merged output sample.
     */
    std::list<const SampleTag*> getSampleTags() const
    {
        return _source.getSampleTags();
    }

    /**
     * Implementation of SampleSource::getSampleTagIterator().
     */
    SampleTagIterator getSampleTagIterator() const
    {
        return _source.getSampleTagIterator();
    }

    /**
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(SampleClient* client) throw()
    {
        _source.addSampleClient(client);
    }

    void removeSampleClient(SampleClient* client) throw()
    {
        _source.removeSampleClient(client);
    }

    /**
     * Add a Client for a given SampleTag.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(SampleClient* client,const SampleTag*) throw()
    {
        // I only have one tag, so just call addSampleClient()
        _source.addSampleClient(client);
    }

    void removeSampleClientForTag(SampleClient* client,const SampleTag*) throw()
    {
        _source.removeSampleClient(client);
    }

    int getClientCount() const throw()
    {
        return _source.getClientCount();
    }

    const SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    /**
     * Connect the resampler to a SampleSource.
     */
    void connect(SampleSource* src) throw(nidas::util::InvalidParameterException);

    void disconnect(SampleSource* src) throw();

    /**
     * Implementation of SampleClient::receive().
     */
    bool receive(const Sample *s) throw();

    /**
     * Implementation of Resampler::flush().
     * Send out whatever samples are available.
     */
    void flush() throw();

private:

    /**
     * Common tasks of constructors.
     */
    void ctorCommon(const std::vector<const Variable*>& vars,bool nansVariable);

    /**
     * Add a SampleTag to this SampleSource.
     */
    void addSampleTag(const SampleTag* tag) throw ()
    {
        _source.addSampleTag(tag);
    }

    void removeSampleTag(const SampleTag* tag) throw ()
    {
        _source.removeSampleTag(tag);
    }

    SampleSourceSupport _source;

    SampleTag _outSample;

    /**                 
     * Requested variables.
     */             
    std::vector<Variable *> _reqVars;

    /**
     * Index of each requested output variable in the output sample.
     */
    std::map<Variable*,unsigned int> _outVarIndices;

    /**
     * For each input sample, first index of variable data values to be
     * read.
     */
    std::map<dsm_sample_id_t,std::vector<unsigned int> > _inmap;

    /**
     * For each input sample, length of variables to read.
     */
    std::map<dsm_sample_id_t,std::vector<unsigned int> > _lenmap;

    /**
     * For each input sample, index into output sample of each variable.
     */
    std::map<dsm_sample_id_t,std::vector<unsigned int> > _outmap;

    unsigned int _ndataValues;

    unsigned int _outlen;

    unsigned int _master;

    int _nmaster;

    dsm_time_t* _prevTT;

    dsm_time_t* _nearTT;

    float* _prevData;

    float* _nearData;

    int* _samplesSinceMaster;

    std::map<dsm_sample_id_t,unsigned int> _ttOutOfOrder;

    bool _debug;

    /**
     * No assignment.
     */
    NearestResampler& operator=(const NearestResampler&);

    /**
     * No copy.
     */
    NearestResampler(const NearestResampler& x);
};

}}	// namespace nidas namespace core

#endif
