/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-03-03 12:46:08 -0700 (Fri, 03 Mar 2006) $

    $LastChangedRevision: 3299 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:5080/svn/nids/branches/ISFF_TREX/dsm/class/SampleSorter.h $
 ********************************************************************

*/

#ifndef DSM_NEARESTRESAMPLER_H
#define DSM_NEARESTRESAMPLER_H

#include <SampleSource.h>
#include <SampleClient.h>
#include <SampleInput.h>
#include <DSMTime.h>

#include <vector>

namespace dsm {

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
class NearestResampler : public SampleClient, public SampleSource {
public:

    /**
     * Constructor.
     */
    NearestResampler(const std::vector<Variable*>& vnames);

    ~NearestResampler();

    bool receive(const Sample *s) throw();

    /**
     * Flush the last sample from the resampler.
     */
    void finish() throw();

    /**
     * Get the SampleTag of my merged output sample.
     */
    const std::set<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

    /**
     * Connect the resampler to an input.
     */
    void connect(SampleInput* input) throw(atdUtil::IOException);

    void disconnect(SampleInput* input) throw(atdUtil::IOException);

private:

    std::set<const SampleTag*> sampleTags;

    SampleTag outSample;

    int nvars;

    int outlen;

    std::map<dsm_sample_id_t,std::vector<int*> > sampleMap;

    int master;

    int nmaster;

    dsm_time_t* prevTT;

    dsm_time_t* nearTT;

    float* prevData;

    float* nearData;

    int* samplesSinceMaster;

    /**
     * No assignment.
     */
    NearestResampler& operator=(const NearestResampler&);

    /**
     * No copy.
     */
    NearestResampler(const NearestResampler& x);
};
}
#endif
