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

class RangeMatcher
{
public:
    using UTime = nidas::util::UTime;

    static const int MATCH_FIRST;
    static const int MATCH_ALL;

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
        dsm1 = dsm1_;
        dsm2 = dsm2_;
        return *this;
    }

    RangeMatcher& set_sid(int sid1_, int sid2_)
    {
        sid1 = sid1_;
        sid2 = sid2_;
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

    /**
     * Return true when @p stime is within the time range or if no time range
     * has been specified.
     */
    bool
    match_time(dsm_time_t stime);

    bool
    match_file(const std::string& filename);

    /**
     * Fill in any MATCH_FIRST references with the given @p dsmid.
     */
    void
    set_first_dsm(int dsmid);

    int dsm1{0};
    int dsm2{0};
    int sid1{MATCH_ALL};
    int sid2{MATCH_ALL};
    dsm_time_t time1{UTime::MIN.toUsecs()};
    dsm_time_t time2{UTime::MAX.toUsecs()};
    std::string file_pattern{};
    bool include{false};
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
     *   [^]{<d1>[-<d2>|*},{<s1>[-<s2>]|*},[<t1>,<t2>],file=<pattern>
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
     * @throws nidas::util::ParseException explaining the parse error
     **/
    void
    addCriteria(const std::string& ctext);

    void
    addCriteria(const RangeMatcher& rm);

    /**
     * Return true if the given @p id satisfies the current range criteria.
     * Search the ranges for one which includes this id, then return true
     * if the range is an inclusion and otherwise false.  The outcome is
     * cached for future lookups, but the cache is cleared if the criteria
     * change.
     **/
    bool
    match(dsm_sample_id_t id);

    /**
     * Return true if this sample matches all the criteria in this matcher,
     * both sample ids and time range.
     **/
    bool
    match(const Sample* samp);

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

private:
    using id_lookup_t = std::unordered_map<dsm_sample_id_t, bool> ;
    using range_matches_t = std::vector<RangeMatcher>;

    range_matches_t _ranges;
    id_lookup_t _lookup;
    nidas::util::UTime _startTime;
    nidas::util::UTime _endTime;
    dsm_sample_id_t _first_dsmid;
};


}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_SAMPLEMATCHER_H
