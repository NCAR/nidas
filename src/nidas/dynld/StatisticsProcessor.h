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

#ifndef NIDAS_DYNLD_STATISTICSPROCESSOR_H
#define NIDAS_DYNLD_STATISTICSPROCESSOR_H

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/dynld/StatisticsCruncher.h>
#include <nidas/util/UTime.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Interface of a processor of samples. A StatisticsProcessor reads
 * input Samples from a single SampleInput, and sends its processed
 * output Samples to one or more SampleOutputs.  
 */
class StatisticsProcessor: public SampleIOProcessor
{
public:

    StatisticsProcessor();

    ~StatisticsProcessor();

    /**
     * Request a sample from this StatisticsProcessor,
     * containing Parameters indicating what variables to
     * process and what kind of statistics to compute.
     */
    void addRequestedSampleTag(SampleTag* tag)
	throw(nidas::util::InvalidParameterException);

    void selectRequestedSampleTags(const std::vector<unsigned int>& sampleIds);

    /**
     * Do common operations necessary when a input has connected:
     * 1. Copy the DSMConfig information from the input to the
     *    disconnected outputs.
     * 2. Request connections for all disconnected outputs.
     *
     * connect() methods in subclasses should do whatever
     * initialization necessary before invoking this
     * StatisticsProcessor::connect().
     */
    void connect(SampleSource*) throw();

    /**
     * Disconnect a SampleInput from this StatisticsProcessor.
     * Right now just does a flush() of all connected outputs.
     */
    void disconnect(SampleSource*) throw();

    /**
     * Implementation of SampleConnectionRequester::connect.
     * Do common operations necessary when a output has connected:
     * 1. do: output->init().
     * 2. add output to a list of connected outputs.
     */
    void connect(SampleOutput* output) throw();

    /**
     * Implementation of SampleConnectionRequester::disconnect.
     * Do common operations necessary when a output has disconnected:
     * 1. do: output->close().
     * 2. remove output from a list of connected outputs.
     */
    void disconnect(SampleOutput* output) throw();

    /**
     * Implementation of SampleSource::flush().
     * Finish the current statistics and send them on.
     */
    void flush() throw();

    void setStartTime(const nidas::util::UTime& val) 
    {
        _startTime = val;
    }

    nidas::util::UTime getStartTime() const
    {
        return _startTime;
    }

    void setEndTime(const nidas::util::UTime& val) 
    {
        _endTime = val;
    }

    nidas::util::UTime getEndTime() const
    {
        return _endTime;
    }

    float getPeriod() const 
    {
        return _statsPeriod;
    }

    /**
     * Whether to generate output samples over time gaps.
     * In some circumstances one might be generating statistics
     * for separate time periods, and one does not want
     * to output samples of missing data for the gaps between
     * the periods.
     */
    bool getFillGaps() const 
    {
        return _fillGaps;
    }

    void setFillGaps(bool val)
    {
        _fillGaps = val;
    }

    /**
     * All output samples (and StatisticsCrunchers) should have a
     * unique name for their counts output variable. This will
     * check if val exists in _cntsNames. If not, it is added to _cntsNames
     * and returned as a unique name. If it is in _cntsName, a suffix
     * of "_N" where N is 1:Inf is appended until it is unique.
     */
    std::string getUniqueCountsName(const std::string& val);

protected:

    /**
     * Implementation of SampleIOProcessor::addSampleTag(SampleTag*).
     */
    /*
    void addSampleTag(SampleTag* tag)
	    throw(nidas::util::InvalidParameterException);
    */

private:

    nidas::util::Mutex _cruncherListMutex;

    std::set<SampleSource*> _connectedSources;

    std::set<SampleOutput*> _connectedOutputs;


    std::list<StatisticsCruncher*> _crunchers;

    struct OutputInfo {
        OutputInfo():
            type(StatisticsCruncher::STATS_UNKNOWN),countsName(),higherMoments(false) {}
        StatisticsCruncher::statisticsType type;
	std::string countsName;
        bool higherMoments;
    };

    std::map<dsm_sample_id_t,struct OutputInfo> _infoBySampleId;

    nidas::util::UTime _startTime;

    nidas::util::UTime _endTime;

    float _statsPeriod;

    bool _fillGaps;

    /**
     * Set of counts variables for output samples.
     */
    std::set<std::string> _cntsNames;

    /**
     * Copy not supported
     */
    StatisticsProcessor(const StatisticsProcessor&);

    /**
     * Assignment not supported
     */
    StatisticsProcessor& operator=(const StatisticsProcessor&);

};

}}	// namespace nidas namespace core

#endif
