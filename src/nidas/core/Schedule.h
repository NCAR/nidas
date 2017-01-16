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
 * ScheduleException is raised when an exception occurs in the
 * Schedule classes.
 **/
class ScheduleException : public std::runtime_error
{
public:
    ScheduleException(const std::string & what):
        std::runtime_error(what)
    {}
};


/**
 * Time specifier strings are broken down into fields, and the class
 * interface provides some operations on them.
 **/
class ScheduleTime
{
public:
    static const int ANYTIME;

    ScheduleTime();

    void
    parse(const std::string& when) throw (ScheduleException);

    /**
     * Figure out if the given time matches this ScheduleTime or not.  They
     * match if all fields are equal which are not ANYTIME.  If there is a
     * match and begin is not null, then set begin to the start of the most
     * recent time period which matches this time and contains when.
     **/
    bool
    match(const nidas::util::UTime& when, nidas::util::UTime* begin = 0);

    bool
    isValid();

    std::ostream&
    toStream(std::ostream& os);

    void
    setFixedTime(const nidas::util::UTime& when);

    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};


/**
 * ScheduleState is a state identifier associated with a time specifier
 * string.
 **/
class ScheduleState
{
public:

    static const std::string ON;
    static const std::string OFF;
    static const std::string DEFAULT;

    ScheduleState(const std::string& id, const std::string& when);

    const std::string&
    getState()
    {
        return _id;
    }

    const std::string&
    getWhen()
    {
        return _when;
    }

    
private:
    std::string _id;
    std::string _when;
};


/**
 * Schedule comprises a set of states and associated times.  Each state
 * applies to the times which match it's time specifier up until the next
 * matching time specifier.  The time specifiers can be absolute, one-time
 * times, meaning the time and date are fully specified, or fields of the
 * time can be left open to match any value of that field.
 *
 * All time specifiers have this format:
 *
 * yyyy-mm-dd,hh:mm:ss
 *
 * Any of those fields can be replaced with an asterisk to match all values
 * for that field.  For example, *-*-*,*:30:00 matches once every half hour on
 * the half hour.  2017-01-15,08:11:10 matches only one time.
 **/
class Schedule
{
public:
    /**
     * Create an empty schedule.
     **/
    Schedule();


    /**
     * Fill a Schedule from a DOM <schedule> element.
     **/
    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

private:
    std::vector<ScheduleState> _states;

};



}}	// namespace nidas namespace core

#endif
