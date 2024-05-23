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

namespace nidas { namespace core {

/**
 * Match samples according to DSM and Sample ID ranges, and configure the
 * ranges with criteria in text format.
 **/
class SampleMatcher
{
    struct RangeMatcher
    {
        static const int MATCH_FIRST = -9;
        static const int MATCH_ALL = -1;

        void
        parse_range(const std::string& rngstr, int& rngid1, int& rngid2);

        bool
        parse_specifier(const std::string& specifier);

        /**
         * Return true when @p dsmid and @p sid are matched by this range.
         */
        bool
        match(int dsmid, int sid);

        /**
         * Fill in any MATCH_FIRST references with the given @p dsmid.
         */
        void
        set_first_dsm(int dsmid);

        int dsm1{0};
        int dsm2{0};
        int sid1{0};
        int sid2{0};
        bool include{false};
    };

public:

    /**
     * Construct an empty SampleMatcher with no ranges.  An empty
     * SampleMatcher implicitly matches all samples.
     **/
    SampleMatcher();

    /**
     * Add a sample range using this syntax:
     * @verbatim
     *   [^]{<d1>[-<d2>|*},{<s1>[-<s2>]|*}
     * @endverbatim
     * The leading '^' will exclude any sample IDs in the given range
     * instead of including them.  The '*' matches any ID.  The older
     * convention where -1 matches all IDs is still supported.
     * 
     * An empty criteria string is allowed and valid but changes nothing.
     **/
    bool
    addCriteria(const std::string& ctext);

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
    typedef std::map<dsm_sample_id_t, bool> id_lookup_t;
    typedef std::vector<RangeMatcher> range_matches_t;

    range_matches_t _ranges;
    id_lookup_t _lookup;
    nidas::util::UTime _startTime;
    nidas::util::UTime _endTime;
    dsm_sample_id_t _first_dsmid;
};


}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_SAMPLEMATCHER_H
