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
#include <Variable.h>

namespace dsm {

class SampleAverager : public SampleClient, public SampleSource {
public:

    SampleAverager();
    virtual ~SampleAverager();

    /**
     * Set average period, in milliseconds.
     */
    void setAveragePeriod(int val) { averagePeriod = val; }

    /**
     * Get average period, in milliseconds.
     */
    int getAveragePeriod() const { return averagePeriod; }

    void init() throw();

    bool receive(const Sample *s) throw();

    void addVariable(const Variable *var);

    void setSampleId(dsm_sample_id_t val) { outSampleId = val; }

    dsm_sample_id_t getSampleId() const { return outSampleId; }

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     */
    void flush() throw (atdUtil::IOException);

protected:
   
    std::list<Variable*> variables;

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

private:

};
}
#endif

