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
#include "nidas/util/Logger.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ios>     // ios::dec
#include <limits>

using std::setw;
using std::setfill;
using std::istringstream;
using namespace nidas::core;

const std::string TimetablePeriod::ON = "on";
const std::string TimetablePeriod::OFF = "off";
const std::string TimetablePeriod::DEFAULT = "";

using std::string;
using nidas::util::UTime;
using nidas::util::UTSeconds;
using nidas::util::InvalidParameterException;

const nidas::util::UTime TimetablePeriod::DEFAULT_START =
    UTime(static_cast<long long>(0));
const nidas::util::UTime TimetablePeriod::DEFAULT_END =
    UTime(std::numeric_limits<long long>::max());

const int TimetableTime::ANYTIME = -1;


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
    bzero(&tm, sizeof(tm));

    tm.tm_year = year - 1900;
    tm.tm_mon = month-1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

    if (year == ANYTIME)
        tm.tm_year = 1970 - 1900;
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

    DLOG(("converted ") << *this << " to start time: "
         << UTime(UTime::fromTm(true, &tm)));
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
toStream(std::ostream& os) const
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


void
TimetablePeriod::
resolve(const nidas::util::UTime& pend,
        const nidas::util::UTime& nstart,
        nidas::util::UTime* begin,
        nidas::util::UTime* end)
{
    UTime tbegin = _start;
    UTime tend = DEFAULT_START;

    // If previous period has no end, and this period has no start,
    // then equate that to an empty period at the end of time.
    if (_start == DEFAULT_START && pend == DEFAULT_END)
    {
        tbegin = DEFAULT_END;
        tend = DEFAULT_END;
    }
    else if (_start == DEFAULT_START)
    {
        tbegin = pend;
    }
    if (tend == DEFAULT_START)
    {
        // If this period has a duration, use it.  Otherwise use the start
        // of the next period.  If there is no start to the next period,
        // then this period goes forever.
        if (_duration)
        {
            tend = tbegin + UTSeconds(_duration);
        }
        else
        {
            tend = nstart;
            if (tend == DEFAULT_START)
            {
                tend = DEFAULT_END;
            }
        }
    }
    if (begin)
        *begin = tbegin;
    if (end)
        *end = tend;
    DLOG(("") << "period " << *this << " with pend=" << pend
         << " and nstart=" << nstart << " resolved to [" << begin
         << "--" << end << "]");
}


bool
TimetablePeriod::
contains(const nidas::util::UTime& pend,
         const nidas::util::UTime& nstart,
         const nidas::util::UTime& when)
{
    UTime begin;
    UTime end;
    resolve(pend, nstart, &begin, &end);
    return (begin <= when && when < end);
}


std::ostream&
TimetablePeriod::
toStream(std::ostream& os) const
{
    os << "tag=" << _tag << "[";
    if (_start == DEFAULT_START)
    {
        os << "open";
    }
    else
    {
        os << _start;
    }
    os << "@" << _duration << "]";
    return os;
}


Timetable::
Timetable():
    _periods()
{
}


void
Timetable::
addPeriod(const TimetablePeriod& period)
{
    _periods.push_back(period);
}


TimetablePeriod
Timetable::
lookupPeriod(const nidas::util::UTime& when)
{
    // For each time period, if the time precedes it, then it is the
    // default.  If it contains it, then use it.  If after checking each
    // time period nothing contains it, then return the default.  Keep
    // track of the running start time of each period by when the current
    // period ends.
    TimetablePeriod result;
    std::ostringstream out;
    out << "Lookup " << when << " in timetable: [";
    unsigned int i = 0;
    UTime lastend = TimetablePeriod::DEFAULT_START;
    for (i = 0; i < _periods.size(); ++i)
    {
        TimetablePeriod& period = _periods[i];
        UTime nextstart = TimetablePeriod::DEFAULT_END;
        // Use start time of next period, if it has one.
        if (i+1 < _periods.size())
        {
            nextstart = _periods[i+1].getStart();
            if (nextstart == TimetablePeriod::DEFAULT_START)
                nextstart = TimetablePeriod::DEFAULT_END;
        }

        UTime begin;
        UTime end;
        period.resolve(lastend, nextstart, &begin, &end);
        if (when < begin)
        {
            // We passed the time, so it belongs to a default time period.
            break;
        }
        if (begin <= when && when < end)
        {
            // Found it.
            result = period;
        }
        // Keep looking.
        lastend = end;
    }
    // If we finish the time periods without a match, then return the
    // default.
    out << "]; returning " << result;
    DLOG(("") << out.str());
    return result;
}


void
Timetable::
fromDOMElement(const xercesc::DOMElement* node)
    throw(nidas::util::InvalidParameterException)
{
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
         child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
            continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

        // Whatever it is, we expect it to have attributes 'start' or
        // 'duration', and the name becomes the tag.
        string tag = elname;
        long duration = 0;
        UTime start = TimetablePeriod::DEFAULT_START;

        string estart = xchild.getAttributeValue("start");
        string eduration = xchild.getAttributeValue("duration");

        if (!eduration.empty())
        {
            // The schema validation means this shouldn't fail.
            istringstream ist(eduration);
            ist >> duration;
            if (ist.fail())
                throw InvalidParameterException
                    ("not a valid value '" + eduration +
                     "' for duration attribute for element: " + elname);
        }
        if (!estart.empty())
        {
            try {
                TimetableTime ttime;
                ttime.parse(estart);
                start = ttime.getStartTime();
            }
            catch (const TimetableException& ttex)
            {
                throw InvalidParameterException
                    ("not a valid value '" + estart +
                     "' for start attribute for element: " + elname);
            }
        }
        
        addPeriod(TimetablePeriod(tag, start, duration));
    }
}
