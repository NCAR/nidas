// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2016 UCAR, NCAR, All Rights Reserved
 ********************************************************************
*/
#ifndef NIDAS_CORE_SAMPLEMATCHER_H
#define NIDAS_CORE_SAMPLEMATCHER_H

#include "SampleTag.h"
#include <nidas/util/UTime.h>

#include <unordered_map>

namespace nidas { namespace core {


/**
 * Match a range of DSM IDs and Sample IDs, with optional time and filename
 * criteria, and optional target ID ranges for remapping.  A default
 * RangeMatcher does not match any samples because it has no DSM ID range, but
 * otherwise it matches all samples, times, and filenames.  SampleMatcher uses
 * multiple RangeMatchers to implement complex selection criteria.  See
 * SampleMatcher::addCriteria() for the text syntax used to configure a
 * RangeMatcher.
 * 
 * ID remapping only happens if target ID ranges are specified and only by a
 * call to remap_ids().  The match methods does not change any IDs.
 **/
class RangeMatcher
{
public:

    struct id_range_t
    {
        id_range_t() = default;

        id_range_t(int id) :
           first(id),
           last(id)
        {}

        id_range_t(int b, int e) :
           first(b),
           last(e)
        {}

        int first{0};
        int last{0};
    };

    using UTime = nidas::util::UTime;

    static const int MATCH_FIRST;
    static const int MATCH_ALL;
    static const dsm_time_t MATCH_ALL_TIME;

    /**
     * Fill this RangeMatcher by parsing @p specifier using the syntax
     * described in SampleMatcher::addCriteria().  Only the values parsed from
     * the string are changed, since this is expected to be called on a
     * RangeMatcher after construction.
     * 
     * @returns this RangeMatcher for chaining or passing directly to
     * addCriteria().
     *
     * @throws nidas::util::ParseException explaining the parse error
     */
    RangeMatcher& parse_specifier(const std::string& specifier);

    RangeMatcher& select(bool inc)
    {
        include = inc;
        return *this;
    }

    RangeMatcher& set_dsm(int dsm1_, int dsm2_)
    {
        dsms.first = dsm1_;
        dsms.last = dsm2_;
        return *this;
    }

    RangeMatcher& set_sid(int sid1_, int sid2_)
    {
        sids.first = sid1_;
        sids.last = sid2_;
        return *this;
    }

    RangeMatcher& set_time(dsm_time_t time1_,
                           dsm_time_t time2_)
    {
        time1 = time1_;
        time2 = time2_;
        return *this;
    }

    RangeMatcher& set_file(const std::string& pattern)
    {
        file_pattern = pattern;
        return *this;
    }

    /**
     * Return true when @p dsmid and @p sid are matched by this range.
     */
    bool
    match(int dsmid, int sid);

    void
    remap_ids(int& dsmid, int& sid);

    /**
     * Return true when @p stime is within the time range or if no time range
     * has been specified, or @p stime is MATCH_ALL_TIME.
     */
    bool
    match_time(dsm_time_t stime);

    /**
     * Return true when the file pattern is found as a substring of @p
     * filename, or when the file pattern or @p filename are empty.
     */
    bool
    match_file(const std::string& filename);

    /**
     * Fill in any MATCH_FIRST references with the given @p dsmid.
     */
    void
    set_first_dsm(int dsmid);

    id_range_t dsms{0};
    id_range_t sids{MATCH_ALL};

    dsm_time_t time1{UTime::MIN.toUsecs()};
    dsm_time_t time2{UTime::MAX.toUsecs()};
    std::string file_pattern{};
    bool include{false};

    id_range_t target_dsms{0};
    id_range_t target_sids{0};
};

/**
 * Match samples according to DSM and Sample ID ranges, and configure the
 * ranges with criteria in text format.
 **/
class SampleMatcher
{
public:

    using RangeMatcher = nidas::core::RangeMatcher;

    /**
     * Construct an empty SampleMatcher with no ranges.  An empty
     * SampleMatcher implicitly matches all samples.
     **/
    SampleMatcher();

    /**
     * Add a sample range using this syntax:
     * @verbatim
     *   [^]{<d1>[-<d2>|*[=D]},{<s1>[-<s2>]|*[=S]},[<t1>,<t2>],file=<pattern>
     * @endverbatim
     * The leading '^' will exclude any sample IDs in the given range
     * instead of including them.  The '*' matches any ID.  The older
     * convention where -1 matches all IDs is still supported.
     *
     * An empty criteria string is allowed and valid but changes nothing.
     *
     * The time range is optional and defaults to all times.  The begin or end
     * time of the range can be omitted, but not both.  Timestamps are
     * accepted in multiple formats, but without any commas or spaces.  The
     * time range is inclusive.  For example,
     * [2023-08-10_12:00:00,2023-08-10_13:00:00] includes samples from 12:00
     * to 13:00 on August 10, 2023 UTC.
     *
     * The file pattern is optional and defaults to all files.  The pattern is
     * not a regular expression, it just must be found as a substring in the
     * name of the input stream, which is the filename in the case of datafile
     * streams. The pattern is case-insensitive.
     *
     * Target DSM and Sample ID ranges can be specified using '=D' and '=S',
     * where D and S are either a single ID or a range using the same syntax
     * as the source. The target range must be a single ID, or it must be the
     * same size as the source range.
     *
     * @throws nidas::util::ParseException explaining the parse error
     **/
    void
    addCriteria(const std::string& ctext);

    void
    addCriteria(const RangeMatcher& rm);

    /**
     * Return true if the given @p id, time @p tt, and @p inputname are
     * selected by the current range criteria.
     **/
    bool
    match(dsm_sample_id_t id, dsm_time_t tt=RangeMatcher::MATCH_ALL_TIME,
          const std::string& inputname="");

    /**
     * Return true if this sample is selected using all the criteria in all
     * the ranges, including id ranges, time range, and filename pattern.  If
     * @p inputname is empty, it matches all file patterns.  This just calls
     * match() using the ID and time from the Sample @p samp.
     **/
    bool
    match(const Sample* samp, const std::string& inputname="");

    /**
     * Like match(), but if the sample matches a range with target IDs, then
     * reassign the sample's ID accordingly.  Returns true if the sample
     * matches, even if the ID is not changed.
     */
    bool
    match_with_reassign(Sample* samp, const std::string& inputname="");

    /**
     * Return true if this matcher can only match a single ID pair
     * (DSM,SID), meaning only one range has been added and it specifies
     * two specific positive IDs.
     **/
    bool
    exclusiveMatch();

    /**
     * The number of ranges added to this SampleMatcher.
     **/
    int 
    numRanges()
    {
        return _ranges.size();
    }

    /**
     * Set the time before which samples will not match.
     **/
    void
    setStartTime(nidas::util::UTime start)
    {
        _startTime = start;
    }

    nidas::util::UTime
    getStartTime()
    {
        return _startTime;
    }

    /**
     * Set the time after which samples will not match.
     **/
    void
    setEndTime(nidas::util::UTime end)
    {
        _endTime = end;
    }

    nidas::util::UTime
    getEndTime()
    {
        return _endTime;
    }

    /**
     * Return the number of samples checked by this SampleMatcher with
     * either match() method.
     */
    unsigned int
    numSamplesChecked()
    {
        return _nsamples;
    }

    /**
     * Return the number samples for which match() returned false.
     */
    unsigned int
    numSamplesExcluded()
    {
        return _nexcluded;
    }

    /**
     * Return the number of samples whose match() result was taken from the
     * cache.
     */
    unsigned int numCacheHits()
    {
        return _ncached;
    }

private:

    bool
    match_range(dsm_sample_id_t id, dsm_time_t tt,
                const std::string& filename, RangeMatcher** rm_out = nullptr);

    using id_lookup_t = std::unordered_map<dsm_sample_id_t, bool> ;
    using range_matches_t = std::vector<RangeMatcher>;

    range_matches_t _ranges;
    id_lookup_t _lookup;
    nidas::util::UTime _startTime;
    nidas::util::UTime _endTime;
    dsm_sample_id_t _first_dsmid;
    bool _all_excludes;
    unsigned int _nsamples;
    unsigned int _nexcluded;
    unsigned int _ncached;

};


}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_SAMPLEMATCHER_H
