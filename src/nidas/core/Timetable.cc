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


#include "Timetable.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ios>     // ios::dec

using std::setw;
using std::setfill;
using std::istringstream;
using namespace nidas::core;

const std::string TimetablePeriod::ON = "on";
const std::string TimetablePeriod::OFF = "off";
const std::string TimetablePeriod::DEFAULT = "";

const int TimetableTime::ANYTIME = -1;


using std::string;
using nidas::util::UTime;


TimetableTime::
TimetableTime(const std::string& when) throw (TimetableException):
    year(ANYTIME),
    month(ANYTIME),
    day(ANYTIME),
    hour(ANYTIME),
    minute(ANYTIME),
    second(ANYTIME)
{
    if (!when.empty())
    {
        parse(when);
    }
}


namespace
{
    int
    parse_time_field(const std::string& name, const std::string& when,
                     string::size_type& begin, char token)
        throw (TimetableException)
    {
        int value = TimetableTime::ANYTIME;
        string::size_type end = when.length();
        if (token)
        {
            end = when.find(token, begin);
            if (end == string::npos)
            {
                throw TimetableException("time field " + name +
                                        " missing separator '" +
                                        token + "': " + when);
            }
        }
        string field = when.substr(begin, end-begin);
        if (field != "*")
        {
            istringstream ist(field);
            ist.setf(std::ios::dec);
            ist >> value;
            if (ist.fail())
            {
                throw TimetableException("could not parse " + name + " field: " +
                                        field);
            }
        }
        begin = end;
        if (token)
            ++begin;
        return value;
    }

    std::ostream&
    fieldToStream(std::ostream& os, int value)
    {
        if (value == TimetableTime::ANYTIME)
        {
            os << "*";
        }
        else if (value > 99)
        {
            os << setw(1) << setfill(' ') << value;
        }
        else
        {
            os << setw(2) << setfill('0') << value;
        }
        os << setw(1) << setfill(' ');
        return os;
    }
}



bool
TimetableTime::
isValid()
{
    bool valid = true;
    valid = valid && (year == ANYTIME || (year > 1900 && year < 10000));
    valid = valid && (month == ANYTIME || (month > 0 && month < 13));
    // Really this could be more accurate, but it's not necessarily an
    // error to match every 31st day of the month, and it would be
    // complicated figuring out whether a 31st day is valid given the rest
    // of the time fields.
    valid = valid && (day == ANYTIME || (day > 0 && day < 32));
    valid = valid && (hour == ANYTIME || (hour >= 0 && hour < 24));
    valid = valid && (minute == ANYTIME || (minute >= 0 && minute < 60));
    // According to tm man page, seconds can be 60 when a leap second
    // happens, but we're not going to allow time specifiers which only
    // match leap seconds.
    valid = valid && (second == ANYTIME || (second >= 0 && second < 60));
    return valid;
}



void
TimetableTime::
parse(const std::string& when) throw (TimetableException)
{
    TimetableTime dtime;

    // format is yyyy-mm-dd,hh:mm:ss
    string::size_type begin = 0;
    // Year is either a number or an asterisk.
    dtime.year = parse_time_field("year", when, begin, '-');
    dtime.month = parse_time_field("month", when, begin, '-');
    dtime.day = parse_time_field("day", when, begin, ',');
    dtime.hour = parse_time_field("hour", when, begin, ':');
    dtime.minute = parse_time_field("minute", when, begin, ':');
    dtime.second = parse_time_field("second", when, begin, 0);
    if (! dtime.isValid())
    {
        throw TimetableException("a time field is out of range: " + when);
    }
    *this = dtime;
}



bool
TimetableTime::
match(const nidas::util::UTime& when)
{
    // Split the time up into components and compare them individually.
    struct tm tm;
    when.toTm(true, &tm);
    bool match = true;
    match = match && (year == ANYTIME || tm.tm_year+1900 == year);
    match = match && (month == ANYTIME || tm.tm_mon+1 == month);
    match = match && (day == ANYTIME || tm.tm_mday == day);
    match = match && (hour == ANYTIME || tm.tm_hour == hour);
    match = match && (minute == ANYTIME || tm.tm_min == minute);
    match = match && (second == ANYTIME || tm.tm_sec == second);

    return match;
}


nidas::util::UTime
TimetableTime::
getStartTime()
{
    struct tm tm;

    tm.tm_year = year+1900;
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

    if (year == ANYTIME)
        tm.tm_year = 1900;
    if (month == ANYTIME)
        tm.tm_mon = 0;
    if (day == ANYTIME)
        tm.tm_mday = 1;
    if (hour == ANYTIME)
        tm.tm_hour = 0;
    if (minute == ANYTIME)
        tm.tm_min = 0;
    if (second == ANYTIME)
        tm.tm_sec = 0;

    return nidas::util::UTime(true, &tm);
}


void
TimetableTime::
setFixedTime(const nidas::util::UTime& when)
{
    struct tm tm;
    when.toTm(true, &tm);
    year = tm.tm_year+1900;
    month = tm.tm_mon+1;
    day = tm.tm_mday;
    hour = tm.tm_hour;
    minute = tm.tm_min;
    second = tm.tm_sec;
}


std::ostream&
TimetableTime::
toStream(std::ostream& os)
{
    std::ostringstream ours;
    fieldToStream(ours, year);
    ours << "-";
    fieldToStream(ours, month);
    ours << "-";
    fieldToStream(ours, day);
    ours << ",";
    fieldToStream(ours, hour);
    ours << ":";
    fieldToStream(ours, minute);
    ours << ":";
    fieldToStream(ours, second);
    os << ours.str();
    return os;
}


TimetablePeriod::
TimetablePeriod(const std::string& tag, const nidas::util::UTime& start,
                long duration):
    _tag(tag),
    _start(start),
    _duration(duration)
{
}

TimetablePeriod::
TimetablePeriod(const std::string& tag, long duration):
    _tag(tag),
    _start(UTime(static_cast<long long>(0))),
    _duration(duration)
{
}

