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

namespace util {
class LogContext;
}

namespace core {
class Variable;
class SampleTracer;
}

namespace dynld { namespace raf {

using namespace nidas::core;

class Aircraft;

class SyncInfo;

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
     * var0[i] are the vector of values of a variable at time i
     * in the record.  The vector may have more then one value,
     * if for example the variable is a histogram.
     *
     * So a sync record is a 4D ragged array with the
     * following dimensions, where last dimension varies most rapidly:
     *
     *      syncrec[nsamples][nvars][nSlots][varlen]
     *
     * nsamples is the number of different sample IDs in the configuration.
     * nvars is the number of variables in a sample.
     * varlen is the number of values for a sample of a variable,
     * usually 1.
     * nSlots is the number of samples per second for a variable/sample,
     * rounded up if necessary.
     * toffset is like an initial variable in the sample with a time
     * and nvalue dimension of 1.
     *
     * Typical sampling rates are:
     *
     * rate	usec/sample     nSlots
     *	1000	1000            1000
     *	100	10000           100
     *	50	20000           50
     *  12.5	80000           13  ARINC (next highest integer)
     *  10	100000          10
     *  8       125000          8
     *  6.25    160000          7   ARINC
     *  3.125   320000          4   ARINC
     *  3       333333 in-exact 3
     *  1.5625  640000          2   ARINC
     *	1	1000000         1
     *
     * The information to unpack a sync record is stored in the
     * header, which is passed first to clients.
     *
     * When a sync record is read, the timetags corresponding to the
     * time slot of each variable value are re-constructed from:
     *  1. sync record sample time, the time at beginning of the second
     *  2. time offset into the second for each sample, in microseconds
     *  3. timeIndex of the variable, from 0 to (N-1)
     *
     * The slot time tags are then:
     *    sampleTime = syncTime + toffset + (timeIndex * dt)
     *
     * If the samples are not actually evenly spaced, then exact time
     * information is lost in resampling into the sync record.  Data samples
     * can be discarded, or slots skipped in the output, if a sample time tag is
     * more than NSLOT_LIMIT*dt from the slot time. 
     *
     * For example, assume these timetags for a 10 Hz variable, with some
     * usual timetag jitter:
     *
     * sample times dataval
     * 00:00:00.09  0
     * 00:00:00.18  1
     * 00:00:00.32  2
     * 00:00:00.60  3 gap, followed by closely spaced samples, typically
     * 00:00:00.62  4 due to DSM latency.
     * 00:00:00.65  5
     * 00:00:00.71  6
     * 00:00:00.79  7
     * 00:00:01.20  8 big gap 0.41 sec
     * 00:00:01.22  9
     *
     * The corresponding values will be put into a row of a sync
     * record whose time is 00:00:00. The time offset for the sample row
     * will be .09 sec (the offset of the first sample).
     *
     * Placing this data in the sync record loses time information
     * for all but the first sample. The slot times in the sync record are
     * (syncTime + toffset + N * dT), where N=[0,9], and dT=0.1 sec:
     *
     * As each sample is received, it is placed in the next slot in the
     * sync record, unless its time tag differs from the slot time by
     * more than NSLOT_LIMIT time dt.
     *
     * syncrec      dataval   time difference from actual sample time
     * 00:00:00.09  0         0.0
     * 00:00:00.19  1        +0.1
     * 00:00:00.29  2        -0.3
     * 00:00:00.39  3        -0.21   value from 00:00:00.60
     * 00:00:00.49  4        -0.13   value from 00:00:00.62
     * 00:00:00.59  5        -0.03   value from 00:00:00.65
     * 00:00:00.69  6        -0.2
     * 00:00:00.79  7         0.0
     * 00:00:00.89  NaN             result of gap, diff of 0.31 sec
     * 00:00:00.99  8        -0.21  value from 00:00:01.20
     * 00:00:01.10  9         0.0   next record, offset of 0.10 sec
     *
     * A previous version of SyncRecordSource computed a slot time index
     * for each sample
     *    timeIndex = (sampleTime - syncTime - toffset + dt/2) / dt
     * This resulted in an more loss of samples over gaps and latency jitter:
     * 00:00:00.09  0    0.0
     * 00:00:00.19  1   +0.1
     * 00:00:00.29  2   -0.3
     * 00:00:00.39  NaN 
     * 00:00:00.49  NaN
     * 00:00:00.59  4   data from 0.62
     * 00:00:00.69  NaN
     * 00:00:00.79  7   data from 0.79
     * 00:00:00.89  NaN
     * 00:00:00.99  NaN
     * 00:00:01.22  NaN
     *
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
     * Construct all the sync record layout artifacts from the list of
     * variables set in _variables.  The layout includes settings like
     * variable lengths, sample indices, sample sizes and offsets, and
     * rates.
     *
     * SyncRecordSource first generates the list of variables with
     * selectVariablesFromProject() prior to calling init().
     **/
    void init();

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

    // static const int NSYNCREC = 2;

    void
    preLoadCalibrations(dsm_time_t sampleTime) throw();

    /**
     * Maximum number of delta-Ts allowed between a sample time tag
     * and its slot in the sync record.
     */
    static const int NSLOT_LIMIT = 2;

    /**
     * Return the index into the next sync record.
     */
    static int nextRecordIndex(int i);

    /**
     * Return the index into the previous sync record. Since
     * currently NSYNCREC==2, this return the same value
     * as nextRecordIndex().
     */
    static int prevRecordIndex(int i);

    /**
     * If the previous sync record is non-null, do
     * sinfo.decrementRecord().
     */
    bool prevRecord(SyncInfo& sinfo);

    /**
     * If the next sync record is non-null, do
     * sinfo.incrementRecord().
     */
    bool nextRecord(SyncInfo& sinfo);

    static const int NSYNCREC = 2;

    /**
     * Which time slot should a sample be placed.
     */
    int computeSlotIndex(const Sample* samp, SyncInfo& sinfo);

private:

    /**
     * @return: true OK, false: cannot place sample in either sync record.
     */
    bool checkTime(const Sample* samp, SyncInfo& sinfo, SampleTracer& stracer,
            nidas::util::LogContext& lc, int warn_times);

    void slog(SampleTracer& stracer, const std::string& msg,
        const Sample* samp, const SyncInfo& sinfo);

    void log(nidas::util::LogContext& lc, const std::string& msg,
            const Sample* samp, const SyncInfo& sinfo);

    void log(nidas::util::LogContext& lc, const std::string& msg,
            const SyncInfo& sinfo);

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
    allocateRecord(int irec, dsm_time_t timetag);

    int advanceRecord(dsm_time_t timetag);


    /**
     * Info kept for each sample in order to assemble and write sync records.
     * Note there is not a default, no-arg constuctor for SyncInfo.
     * So you can't do:
     *      SyncInfo& sinfo = _syncInfo[id];
     * which would need the no-arg constructor if the element is not found.
     * Instead, do:
     *      map<dsm_sample_id_t, SyncInfo>::iterator si = _syncInfo.find(id);
     *      if (si != _syncInfo.end()) {
     *          SyncInfo& sinfo = si->second;  // use reference to avoid copy
     *          ...
     *      }
     */
    std::map<dsm_sample_id_t, SyncInfo> _syncInfo;

    /**
     * List of all variables in the sync record.
     */
    std::list<const Variable*> _variables;

    SampleTag _syncRecordHeaderSampleTag;

    SampleTag _syncRecordDataSampleTag;

    size_t _recSize;

    dsm_time_t _syncHeaderTime;

    dsm_time_t _syncTime[2];

    /**
     * Index of current sync record.
     */
    int _current;

    int _halfMaxUsecsPerSample;

    SampleT<double>* _syncRecord[2];

    double* _dataPtr[2];

    size_t _unrecognizedSamples;

    std::ostringstream _headerStream;

    const Aircraft* _aircraft;

    bool _initialized;

    int _unknownSampleType;

    unsigned int _badLaterSamples;

    /** No copying. */
    SyncRecordSource(const SyncRecordSource&);

    /** No assignment. */
    SyncRecordSource& operator=(const SyncRecordSource&);

};

/*
 * Whether to define an explicit copy constructor
 * and assignment op for SyncInfo. Some care is taken
 * in the code to avoid copy and assignment. Define
 * this to detect how often they are called.
 */
#define EXPLICIT_SYNCINFO_COPY_ASSIGN

/**
 * Parameters needed for each sample to assemble and
 * write a sync record.
 */
class SyncInfo
{
public:

    /**
     * Constructor.  Note there is not a default, no-arg constuctor.
     */
    SyncInfo(dsm_sample_id_t i, float r, SyncRecordSource* srs);

#ifdef EXPLICIT_SYNCINFO_COPY_ASSIGN
    /**
     * Copy constructor.
     */
    SyncInfo(const SyncInfo&);
    static unsigned int ncopy;

    /**
     * Assignment
     */
    SyncInfo& operator=(const SyncInfo&);
    static unsigned int nassign;
#endif

    void addVariable(const Variable* var);

    void advanceRecord(int last);

    dsm_sample_id_t id;

    /**
     * Sampling rate of the sample.
     */
    float rate;

    /**
     * Smallest integer not less than rate, computed as ceil(rate).
     * The number of slots for the sample in the sync record.
     * For example, 13 for a sample rate of 12.5.
     */
    int nSlots;

    /**
     * Number of microseconds per sample,
     * 1000000/rate, rounded to an integer.
     */
    int dtUsec;

private:
    /**
     * Index of next slot for the sample in the current sync record.
     */
    int islot;

    /**
     * Index of current sync record for this sample.
     */
    int irec;

public:

    int getSlotIndex() const { return islot; }

    bool incrementSlot();

    bool decrementSlot();

    bool checkNonIntRateIncrement();

    void computeSlotIndex(const Sample* samp);

    int getRecordIndex() const { return irec; }

    void incrementRecord()
    {
        irec = _srs->nextRecordIndex(irec);
    }

    void decrementRecord()
    {
        incrementRecord();
    }

    /**
     * Number of values for each variable in the sample.
     */
    std::vector<size_t> varLengths;

    /**
     * Number of data values in one second: nSlots times the sum of
     * the varLengths for the variables in the sample plus one for
     * the time offset.
     */
    size_t sampleLength;

    /**
     * Offset of this sample in the sync record.  The first
     * value for the sample is the time offset within the second,
     * followed by the data values for each variable.
     */
    size_t sampleOffset;

    /**
     * Variables in the sample.
     */
    std::list<const Variable*> variables;

    /**
     * Offsets of the each variable in the sync record.
     */
    std::vector<size_t> varOffsets;

    unsigned int discarded;

    bool overWritten;

    unsigned int noverWritten;

    unsigned int nskips;

    unsigned int skipped;

    unsigned int total;

    /**
     * See the comment below about minDiff. The minimum difference will
     * never be more than minDiffInit.
     * For integral sample rates it is simply
     *      minDiffInit = dtUsec
     * For non-integral rates it can be either
     *      minDiffInit = dtUsec
     *  or
     *      minDiffInit = (nSlots * dtUsec) % USECS_PER_SEC
     *  The first value results in output sample tags that are closer to the original
     *  samples with cleaner delta-Ts between non-nan values, but last slot time in
     *  a sync record may be in the next second. We now believe this isn't
     *  an issue with nimbus, so we'll go with minDiffInit = dtUsec.
     */
    int minDiffInit;

    /**
     * The minimum difference between the sample time tags and their
     * corresponding slot times is computed over the second.
     * This value is written into the sync record as the offset for the
     * samples within that second.
     */
    int minDiff;

    /**
     * skipMod provides a way to insert NaNs in sync records
     * with non-integral rates.
     * In incrementCount(), if the next slot to be written is
     * the last in the record and the modulus (sampCount % skipMod)
     * is non-zero, then the last slot in the record is skipped, leaving
     * a NaN.
     *
     * skipMod is initialized to (int)(1.0 / (1 - (nSlots-rate)))
     * Here are the values for some expected rates, including
     * non-integral ARINC rates.
     *
     * rate     skipMod   (skipModCount % skipMod) ! = 0
     * integral 1         never true, no skips
     * 12.5     2         skip slot every other second
     * 6.25     4         skip slot in 3 out of 4 seconds
     * 3.125    8         skip slot in 7 out of 8
     * 1.5625   1         never true (algorithm breaks down)
     *
     */
    int skipMod;

    /**
     * Sample counter used for non-integral rates.
     */
    int skipModCount;

    /**
     * If sample time tag differs from the slot time by this much or more
     * then increment nEarlySamp or nLateSamp.
     */
    static const int TDIFF_CHECK_USEC = 2 * USECS_PER_MSEC;

    /**
     * How many successive samples have been earlier than their slot time
     * by more than TDIFF_CHECK_USEC.
     */
    unsigned int nEarlySamp;

    /**
     * How many successive samples have been later than their slot time
     * by more than TDIFF_CHECK_USEC.
     */
    unsigned int nLateSamp;

    /**
     * Once nEarlySamp or nLateSamp exceed this value, adjust the slot index.
     */
    unsigned int outOfSlotMax;

private:

    SyncRecordSource* _srs;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
