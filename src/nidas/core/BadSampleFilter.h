// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2016 UCAR, NCAR, All Rights Reserved
 ********************************************************************
*/
#ifndef NIDAS_CORE_BADSAMPLEFILTER_H
#define NIDAS_CORE_BADSAMPLEFILTER_H

#include <iosfwd>

#include "SampleTag.h"
#include <nidas/util/UTime.h>
#include "NidasApp.h"

namespace nidas { namespace core {

/**
 * Implement rules for filtering bad samples.  The parameters are public to
 * make this object easy and efficient to embed in other classes.
 **/
class BadSampleFilter
{
    typedef nidas::util::UTime UTime;

public:

    BadSampleFilter();

    /**
     * Enable filtering of bad samples.  A sample is invalid if the sample
     * type is out of range, or if the DSM ID, sample length, or sample
     * time fall out their specified valid ranges.  Other methods change
     * the valid ranges.  As a convenience, calling any of the methods to
     * change the range filters automatically enables filtering, same as
     * calling setFilterBadSamples() directly.  Once filtering is enabled
     * by any of those methods, it can only be disabled by calling
     * setFilterBadSamples(false).  It is a mistake to use these methods to
     * skip otherwise valid samples.  When sample is invalid, the
     * SampleInputStream loses sync with the sample stream and begins
     * looking for another valid sample one byte at a time, possibly
     * discovering a sample which meets the valid criteria but was not an
     * actual sample.
     **/
    void setFilterBadSamples(bool val);

    bool filterBadSamples() const
    {
        return _filterBadSamples;
    }

    /**
     * The default minimum DSM ID for a valid sample is 1.
     * See setFilterBadSamples().
     **/
    void setMinDsmId(unsigned int val);

    unsigned int minDsmId() const
    {
        return _minDsmId;
    }

    /**
     * The default maximum DSM ID for a valid sample is 1024.
     * See setFilterBadSamples().
     **/
    void setMaxDsmId(unsigned int val);

    unsigned int maxDsmId() const
    {
        return _maxDsmId;
    }

    /**
     * The default minimum sample length is 1.
     * See setFilterBadSamples().
     **/
    void setMinSampleLength(unsigned int val);

    unsigned int minSampleLength() const
    {
        return _minSampleLength;
    }

    /**
     * The default maximum sample length is UINT_MAX.
     * See setFilterBadSamples().
     **/
    void setMaxSampleLength(unsigned int val);

    unsigned int maxSampleLength() const
    {
        return _maxSampleLength;
    }
    
    /**
     * Set the minimum sample time for a valid sample.  See
     * setFilterBadSamples().
     **/
    void setMinSampleTime(const nidas::util::UTime& val);

    UTime minSampleTime() const
    {
        return _minSampleTime;
    }

    /**
     * Set the maximum sample time for a valid sample.  See
     * setFilterBadSamples().
     **/
    void setMaxSampleTime(const nidas::util::UTime& val);

    UTime maxSampleTime() const
    {
        return _maxSampleTime;
    }

    /**
     * Parse the filter rule string and throw NidasAppException if it does
     * not parse.  An empty rule changes nothing.
     *
     * Rules are a comma-separated list of key=value settings using these
     * keys: maxdsm,mindsm,maxtime,mintime,maxlen,minlen,on,off.  Any of
     * the keys may be specified in any order.  Once enabled, these are the
     * defaults: maxdsm=1024,mindsm=1,maxlen=32768,minlen=1.
     *
     * Times are specified in any format that UTime can parse by default,
     * such as ISO time, YYYY-mm-ddTHH:MM:SS.fff, or condensed time
     * YYYYmmddHHMMSS.fff.  The time string cannot contain any commas.
     *
     * Additionally the keys 'on' and 'off' enable or disable filtering.
     * So a rule of just 'on' enables the default filtering.  Setting any
     * of the rules implicitly enables filtering.
     **/
    void
    setRules(const std::string& rule);

    /**
     * Return true if @sheader matches any of the criteria for an invalid
     * sample.
     **/
    bool
    invalidSampleHeader(const SampleHeader& sheader);

    bool
    operator==(const BadSampleFilter& right) const;

private:
    
    /**
     * Set a single filter setting as a string in the form 'on', 'off', or
     * <field>=<value>.  See setRules().  Like setRules(), throw
     * NidasAppException if the rule cannot be parsed.  This is an internal
     * implementation method.  Anything valid for setRule() can be passed
     * to setRules() instead.
     **/
    void
    setRule(const std::string& rule);

    bool _filterBadSamples;

    unsigned int _minDsmId;
    unsigned int _maxDsmId;

    size_t _minSampleLength;
    size_t _maxSampleLength;

    dsm_time_t _minSampleTime;
    dsm_time_t _maxSampleTime;
};


/**
 * Stream the current rules for BadSampleFilter @p bsf to @p out.
 **/
std::ostream&
operator<<(std::ostream& out, const BadSampleFilter& bsf);



inline bool
BadSampleFilter::
invalidSampleHeader(const SampleHeader& sheader)
{
    if (_filterBadSamples)
    {
        // If filtering enabled but no max time set, use now plus 1 day.
        if (_maxSampleTime == LONG_LONG_MAX)
            _maxSampleTime = UTime().toUsecs() + 24*USECS_PER_HOUR;
        // Likewise if no min time set, use within 20 years of max time.
        if (_minSampleTime == LONG_LONG_MIN)
            _minSampleTime = _maxSampleTime - 20*365*USECS_PER_DAY;
    }

    // screen bad headers.
    return _filterBadSamples &&
        (sheader.getType() >= UNKNOWN_ST ||
         GET_DSM_ID(sheader.getId()) < _minDsmId ||
         GET_DSM_ID(sheader.getId()) > _maxDsmId ||
         sheader.getDataByteLength() < _minSampleLength ||
         sheader.getDataByteLength() > _maxSampleLength ||
         sheader.getTimeTag() < _minSampleTime ||
         sheader.getTimeTag() > _maxSampleTime);
}


class BadSampleFilterArg : public nidas::core::NidasAppArg
{
public:
    BadSampleFilterArg();

    BadSampleFilter&
    getFilter()
    {
        return _bsf;
    }

    virtual bool
    parse(const ArgVector& argv, int* argi = 0);

private:
    BadSampleFilter _bsf;

};



}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_BADSAMPLEFILTER_H
