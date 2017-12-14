// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_SYNCRECORDSOURCE_H
#define NIDAS_CORE_SYNCRECORDSOURCE_H

#include <nidas/core/Resampler.h>
#include <nidas/core/SampleTag.h>

#define SYNC_RECORD_ID 3
#define SYNC_RECORD_HEADER_ID 2

namespace nidas {

namespace core {
class Variable;
}

namespace dynld { namespace raf {

using namespace nidas::core;

class Aircraft;

class SyncRecordSource: public Resampler
{
    /**
     * SyncRecordSource builds "sync records" from a sample stream.
     * This is a lossy process whete data is being munged into a one-second,
     * ragged matrix of double precision values, called a sync record. 
     * Each row of the matrix contains the data for the variables with
     * a given sample id. In NIDAS, a sample is a group of variables
     * all with the same timetag.  In order to build sync records
     * a sample also must have a known sampling rate, SampleTag::getRate().
     *
     * Each row of the matrix is formatted as follows, where the
     * sample row includes variables var0, var1, etc:
     *      toffset, var0[0], var0[1], ... var0[N-1], var1[0], var1[1], ...
     * toffset is the number of microseconds into the second to
     * associate with the zeroth value of each variable.
     * N is the sampling rate (#/sec) of all the variables
     * in the sample, rounded up to the next highest integer if the
     * rate is not integral.
     * var[i] are the vector of values of the variable at time i
     * in the record.  The vector may have more then one element,
     * if for example the variable is a histogram.
     *
     * So a sync record is a 4D ragged array with the
     * following dimensions, where time varies most rapidly:
     *
     *      syncrec[sample][var][time][element]
     *
     * The number of elements may vary with each variable. The
     * time dimension can vary between samples.
     * toffset is like an initial variable in the sample with a time
     * and element dimension of 1.
     *
     * Typical sampling rates are:
     *
     * rate	usec/sample     N
     *	1000	1000            1000
     *	100	10000           100
     *	50	20000           50
     *  12.5	80000           13 (next highest integer)
     *  10	100000          10
     *  8       125000          8
     *  3       333333 in-exact 3
     *	1	1000000         1
     *
     * When a sync record is read, the timetags of the variables
     * are re-constructed with the following information:
     *  1. sync record sample time, the time at beginning of the second
     *  2. time offset into the second for each sample, in microseconds
     *  3. timeIndex of the variable, from 0 to (N-1)
     *
     * The information to unpack a sync record is stored in the
     * header, which is passed first to clients.
     *
     * The timetags for each sample of a variable are then:
     *    sampleTime = syncTime + toffset + (timeIndex * usecsPerSamp)
     * Inverting this to compute the timeIndex:
     *    timeIndex = (sampleTime - syncTime - toffset) / usecsPerSamp
     * So that all samples, plus or minus 1/2 sample delta-T, are given
     * the same timeIndex, we use:
     *    timeIndex =
     *    (sampleTime - syncTime - toffset + usecsPerSamp/2) / usecsPerSamp
     *
     * If the samples are not actually evenly spaced, then exact time
     * information is lost in resampling into the sync record.  Data can also
     * be lost, if two samples are near in time and so have the same
     * resulting timeIndex.
     *
     * For example, assume these timetags for a 10 Hz variable, with some
     * usual timetag jitter:
     *
     * sample times
     * 00:00:00.09
     * 00:00:00.18
     * 00:00:00.32
     * 00:00:00.38
     * 00:00:00.50
     * 00:00:00.52  major timetag jitter
     * 00:00:00.71
     * 00:00:00.79
     * 00:00:00.92
     * 00:00:01.01
     *
     * The corresponding values will be put into a row of a sync
     * record whose time is 00:00:00. The time offset for the sample row
     * will be .09 sec (the offset of the first sample).
     *
     * Placing this data in the sync record looses time information
     * for all but the first sample. The re-created timetags will be
     * (syncTime + toffset + N * dT), where N=[0,9], and dT=0.1 sec:
     *
     * re-created times difference(sec)
     * 00:00:00.09       0.0
     * 00:00:00.19      +0.1
     * 00:00:00.29      -0.3
     * 00:00:00.39      +0.1
     * 00:00:00.49      -0.3 will contain value from 00:00:00.52
     * 00:00:00.59      data will be NaN, a loss of data
     * 00:00:00.69      -0.2
     * 00:00:00.79       0.0
     * 00:00:00.89      -0.3
     * 00:00:00.99      -0.2
     *
     * Note that the original time tag for the last value was in the
     * next second, but is placed in the sync record for the previous
     * second.
     * The received samples are sorted in time before they are
     * placed in the sync record, but because a sample from the
     * next second may be placed in the previous sync record, we have
     * maintain two sync records as they are filled.
     */
public:
    
    SyncRecordSource();

    virtual ~SyncRecordSource();

    /**
     * This method and selectVariablesFromSensor() are used to select the
     * list of variables from a Project configuration in order of sensor,
     * with variables in order for each sensor, accepting only the
     * variables which make sense for Aircraft SyncRecords.  The variables
     * are appended to the @p variables list.
     *
     * Rather than rely on the sample tags from the source, this allows a
     * single function to be shared to accumulate processed sample tags and
     * variables directly from the Project.  Applications (like nimbus) can
     * use this to get the same list of Variables as would be retrieved from
     * SyncRecordReader, except they include all the metadata directly from
     * the Project instead of the sync record header.
     **/
    static void
    selectVariablesFromProject(Project* project, 
                               std::list<const Variable*>& variables);

    /**
     * See selectVariablesFromProject().
     **/
    static void
    selectVariablesFromSensor(DSMSensor* sensor, 
                              std::list<const Variable*>& variables);

    SampleSource* getRawSampleSource() { return 0; }

    SampleSource* getProcessedSampleSource() { return &_source; }

    /**
     * Get the output SampleTags.
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

    /**
     * Implementation of Resampler::flush().
     * Send current sync record, whether finished or not.
     */
    void flush() throw();

    const SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    void connect(SampleSource* source) throw();

    void disconnect(SampleSource* source) throw();

    /**
     * Generate and send a sync record header sample.  The sync record
     * should have been laid out already, but that happens when a source is
     * connected with connect(SampleSource*).  Typically sendSyncHeader()
     * should be called after clients are connected with addSampleClient().
     * The header time tag is the time of the first raw sample read from
     * the input stream, set in the call to preLoadCalibrations().  This is
     * not to be confused with a NIDAS stream header, like would be
     * generated by a HeaderSource, thus the distinction in the name.
     **/
    void sendSyncHeader() throw();

    bool receive(const Sample*) throw();

    static const int NSYNCREC = 2;

    void
    preLoadCalibrations(dsm_time_t sampleTime) throw();

protected:

    void init();

private:

    SampleSourceSupport _source;

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

    void createHeader(std::ostream&) throw();

    void
    sendSyncRecord();

    void
    allocateRecord(int isync, dsm_time_t timetag);

    int advanceRecord(dsm_time_t timetag);

    int
    sampleIndexFromId(dsm_sample_id_t sampleId);

    /**
     * Construct all the sync record layout artifacts from the list of
     * variables set in _variables.  The layout includes settings like
     * variable lengths, sample indices, sample sizes and offsets, and
     * rates.
     *
     * SyncRecordSource first generates the list of variables with
     * selectVariablesFromProject() prior to calling layoutSyncRecord().
     **/
    void
    layoutSyncRecord();

    /**
     * A vector, with each element being a list of variables from a
     * sample. The order of elements in the vector is the order
     * of the variables in the sync record. By definition, every
     * variable in a sample has the same sampling rate.
     */
    std::vector<std::list<const Variable*> > _varsByIndex;

    /**
     * A mapping between sample ids and sample indices. Sample indices
     * range from 0 to the total number of different input samples -1.
     * When we receive a sample, what is its sampleIndex.
     */
    std::map<dsm_sample_id_t, int> _sampleIndices;

    /**
     * For each sample, by its index, the sampling rate, rounded up to an
     * integer.
     */
    std::vector<int> _intSamplesPerSec;

    /**
     * For each sample, the sampling rate, in floats.
     */
    std::vector<float> _rates;

    /**
     * For each sample, number of microseconds per sample,
     * 1000000/rate, truncated to an integer.
     */
    std::vector<int> _usecsPerSample;

    int _halfMaxUsecsPerSample;

    /**
     * For the first sample of each variable, its time offset
     * in microseconds into the second.
     */
    std::vector<int> _offsetUsec[2];

    /**
     * Number of values of each sample in the sync record.
     * This will be the rate * the number of values in each sample.
     */
    std::vector<size_t> _sampleLengths;

    /**
     * For each sample, its offset into the whole record.
     */
    std::vector<size_t> _sampleOffsets;

    /**
     * Offsets into the sync record of each variable in a sample,
     * indexed by sampleId.
     */
    std::vector<int*> _varOffsets;

    /**
     * Lengths of each variable in a sample,
     * indexed by sampleId.
     */
    std::vector<size_t*> _varLengths;

    /**
     * Number of variables in each sample.
     */
    std::vector<size_t> _numVars;

    /**
     * List of all variables in the sync record.
     */
    std::list<const Variable*> _variables;

    SampleTag _syncRecordHeaderSampleTag;

    SampleTag _syncRecordDataSampleTag;

    int _recSize;

    dsm_time_t _syncHeaderTime;

    dsm_time_t _syncTime[2];

    /**
     * Index of current sync record.
     */
    int _current;

    SampleT<double>* _syncRecord[2];

    double* _dataPtr[2];

    size_t _unrecognizedSamples;

    std::ostringstream _headerStream;

    int _badLaterTimes;

    int _badEarlierTimes;

    const Aircraft* _aircraft;

    bool _initialized;

    int _unknownSampleType;

    /** No copying. */
    SyncRecordSource(const SyncRecordSource&);

    /** No assignment. */
    SyncRecordSource& operator=(const SyncRecordSource&);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
