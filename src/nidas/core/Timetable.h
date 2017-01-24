// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2017, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_CORE_SCHEDULE_H
#define NIDAS_CORE_SCHEDULE_H

#include <vector>
#include <string>
#include <stdexcept>
#include <iosfwd>

#include "DOMable.h"
#include "nidas/util/UTime.h"

namespace nidas { namespace core {



/**
 * TimetableException is raised when an exception occurs in the Timetable
 * classes.
 **/
class TimetableException : public std::runtime_error
{
public:
    TimetableException(const std::string & what):
        std::runtime_error(what)
    {}
};


/**
 * Time specifier strings are broken down into fields, and the class
 * interface provides some operations on them.
 **/
class TimetableTime
{
public:
    static const int ANYTIME;

    /**
     * Construct a TimetableTime from the given string @p when with the
     * same syntax as @p parse().
     **/
    TimetableTime(const std::string& when = "") throw (TimetableException);

    void
    parse(const std::string& when) throw (TimetableException);

    /**
     * Figure out if the given time matches this TimetableTime or not.
     * They match if all fields are equal which are not ANYTIME.
     **/
    bool
    match(const nidas::util::UTime& when);

    /**
     * Generate the absolute UTime which this time first matches.  Any
     * fields set to ANYTIME will be taken as the value of the earliest
     * possible value for that period.  If all the fields are ANYTIME, then
     * the start time is 1900-01-01,00:00:00.
     **/
    nidas::util::UTime
    getStartTime();

    bool
    isValid();

    std::ostream&
    toStream(std::ostream& os) const;

    void
    setFixedTime(const nidas::util::UTime& when);

    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};


inline std::ostream&
operator<<(std::ostream& out, const TimetableTime& ttime)
{
    return ttime.toStream(out);
}


/**
 * TimetablePeriod is a tag associated with a period time of time in a
 * timetable.
 *
 * Start time specifiers have this format:
 *
 * yyyy-mm-dd,hh:mm:ss
 *
 **/
class TimetablePeriod
{
public:

    static const std::string ON;
    static const std::string OFF;
    static const std::string DEFAULT;

    static const nidas::util::UTime DEFAULT_START;
    static const nidas::util::UTime DEFAULT_END;

    /**
     * Create a TimetablePeriod with the given @p tag and @p duration in
     *  seconds.  When tag is empty and duration is zero, that is the
     *  default constructor, which creates a time period with no tags, no
     *  start time, and infinite duration.
     **/
    TimetablePeriod(const std::string& tag = DEFAULT, long duration = 0);

    /**
     * Create a TimetablePeriod with the given @p tag and @p start time,
     * and optionally limit the time period to the given @p duration.
     **/
    TimetablePeriod(const std::string& tag,
                    const nidas::util::UTime& start,
                    long duration = 0);


    const std::string&
    getTag()
    {
        return _tag;
    }

    const nidas::util::UTime&
    getStart()
    {
        return _start;
    }

    /**
     * Return the duration of this timetable period in seconds.  A zero
     * duration means this period is not limited.
     **/
    long
    getDuration()
    {
        return _duration;
    }
    
    /**
     * Given the end time of a previous period and the start time of the
     * following period, determine the begin and end time of this period.
     * The end of the previous period is used as the start of this one if
     * no start time is set, and the beginning of the next period is used
     * if this period has no duration.
     **/
    void
    resolve(const nidas::util::UTime& pend,
            const nidas::util::UTime& nstart,
            nidas::util::UTime* begin,
            nidas::util::UTime* end);
            
    /**
     * Use resolve() to compute begin and end times for this
     * TimetablePeriod relative to the previous end and the next start,
     * then return true if the time @p when is between those times.
     **/
    bool
    contains(const nidas::util::UTime& pend,
             const nidas::util::UTime& nstart,
             const nidas::util::UTime& when);

    std::ostream&
    toStream(std::ostream& os) const;

private:

    std::string _tag;
    nidas::util::UTime _start;
    long _duration;
};


inline std::ostream&
operator<<(std::ostream& out, const TimetablePeriod& period)
{
    return period.toStream(out);
}


/**
 * Timetable is a sequence of TimetablePeriod instances.  Given a specific
 * point in time, it can return any tag that was active during that time.
 **/
class Timetable
{
public:
    /**
     * Create an empty schedule.
     **/
    Timetable();

    /**
     * Add the @p period to the end of this Timetable.
     **/
    void
    addPeriod(const TimetablePeriod& period);

    /**
     * Look up the given time in the table and return the TimetablePeriod
     * which covers that time.  If the time is not contained in any time
     * period, return a default TimetablePeriod.
     **/
    TimetablePeriod
    lookupPeriod(const nidas::util::UTime& when);
    
    /**
     * Convenience method which directly returns the tag of whatever
     * TimetablePeriod is returned by lookupPeriod().
     **/
    std::string
    lookupTag(const nidas::util::UTime& when)
    {
        return lookupPeriod(when).getTag();
    }

    /**
     * Fill a Timetable from a DOM <schedule> element.
     **/
    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

private:
    std::vector<TimetablePeriod> _periods;

};



}}	// namespace nidas namespace core

#endif
