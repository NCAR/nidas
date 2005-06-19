/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLEAVERAGER_H
#define DSM_SAMPLEAVERAGER_H

#include <SampleSource.h>
#include <SampleClient.h>
#include <SampleTag.h>
#include <Variable.h>
#include <DSMTime.h>

namespace dsm {

class SampleAverager : public SampleClient, public SampleSource {
public:

    SampleAverager();

    SampleAverager(const SampleAverager&);

    virtual ~SampleAverager();

    /**
     * Set average period.
     * @param val average period, in milliseconds.
     */
    void setAveragePeriod(int val) {
        averagePeriod = val * USECS_PER_MSEC;
	sampleTag.setRate((float)val / MSECS_PER_SEC);
    }

    /**
     * Get average period.
     * @return average period, in milliseconds.
     */
    int getAveragePeriod() const { return averagePeriod / USECS_PER_MSEC; }

    void init() throw();

    bool receive(const Sample *s) throw();

    void addVariable(const Variable *var);

    void setSampleId(dsm_sample_id_t val) { outSampleId = val; }

    dsm_sample_id_t getSampleId() const { return outSampleId; }

    const SampleTag* getSampleTag() const { return &sampleTag; }

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     */
    void flush() throw ();

protected:
   
    /**
     * Length of average, in milliseconds.
     */
    int averagePeriod;

    dsm_sample_id_t outSampleId;

    dsm_time_t endTime;

    std::map<dsm_sample_id_t,std::vector<int> > inmap;
    std::map<dsm_sample_id_t,std::vector<int> > outmap;

    int nvariables;

    double *sums;

    int *cnts;

    SampleTag sampleTag;

private:

};
}
#endif

