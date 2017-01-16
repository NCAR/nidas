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


#include "Schedule.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ios>     // ios::dec

using std::setw;
using std::setfill;
using std::istringstream;
using namespace nidas::core;

const std::string ScheduleState::ON = "on";
const std::string ScheduleState::OFF = "off";
const std::string ScheduleState::DEFAULT = "default";

const int ScheduleTime::ANYTIME = -1;


using std::string;


ScheduleTime::
ScheduleTime() :
    year(ANYTIME),
    month(ANYTIME),
    day(ANYTIME),
    hour(ANYTIME),
    minute(ANYTIME),
    second(ANYTIME)
{
}


namespace
{
    int
    parse_time_field(const std::string& name, const std::string& when,
                     string::size_type& begin, char token)
        throw (ScheduleException)
    {
        int value = ScheduleTime::ANYTIME;
        string::size_type end = when.length();
        if (token)
        {
            end = when.find(token, begin);
            if (end == string::npos)
            {
                throw ScheduleException("time field " + name +
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
                throw ScheduleException("could not parse " + name + " field: " +
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
        if (value == ScheduleTime::ANYTIME)
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
ScheduleTime::
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
ScheduleTime::
parse(const std::string& when) throw (ScheduleException)
{
    ScheduleTime dtime;

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
        throw ScheduleException("a time field is out of range: " + when);
    }
    *this = dtime;
}



bool
ScheduleTime::
match(const nidas::util::UTime& when, nidas::util::UTime* begin = 0)
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

    // Now if there's match, figure out when the time period begins which
    // first matches this time and contains when.  So we set all the fields
    // to match when, but then the rightmost (least significant) time field
    // with ANYTIME is set to it's starting index, and those fields are
    // converted to a UTime.
    //
    // But then how do set a time which takes place once a day?
    //
    // *-*-*,12:00:00
    //
    // How about once every hour?
    //
    // *-*-*,*:00:00
    //
    // The start of every minute during hour 12.
    // 
    // *-*-*,12:*:00
    //
    // So if we want to turn something one and off alternating every hour?
    //
    // on   *-*-*,00:00:00
    // off  *-*-*,01:00:00
    // on   *-*-*,02:00:00
    // off  *-*-*,04:00:00
    // on   *-*-*,05:00:00
    // off  *-*-*,06:00:00
    // on   *-*-*,07:00:00
    //
    // Seems verbose.  We may want to define a schedule like in the sensor
    // catalog.

    // The notion above contradicts the intention of the match() method,
    // since a time in the first hour but not exactly 00:00:00 would not
    // match it.  We don't really want to know if a time matches so much as
    // what the begin time is of a period which contains it, if any.

    // Further, how to turn off an intermittent schedule when it no longer
    // applies?  For example, once the days get long enough, stop turning a
    // sensor off regularly.  A schedule event needs an anchor time, and
    // that event ends when the next schedule starts?  Or maybe a fixed
    // time is used in the schedule to separate different schedules?

    // off when=<fixed>
    // on period="2 hours"

    // A schedule should be able to contain a loop of states with a
    // duration:

    // on/off when=<fixed>
    // <loop when='<fixed>'>
    //   <on duration="2 hours"><off duration="1 hour">
    // </loop>

    // Even though chronological order may not be strictly necessary, it
    // might be a good idea to warn when it happens.


    if (begin)
    {
        ScheduleTime dtime;
        dtime.setFixedTime(when);
        if (second == ANYTIME)
            ...

    }

    return match;
}


void
ScheduleTime::
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
ScheduleTime::
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


ScheduleState::
ScheduleState(const std::string& id, const std::string& when) :
    _id(id),
    _when(when)
{
}



