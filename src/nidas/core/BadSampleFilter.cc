// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:

#include "BadSampleFilter.h"
#include <functional>

using nidas::core::BadSampleFilter;
using nidas::core::BadSampleFilterArg;
using nidas::util::UTime;
using nidas::core::NidasAppException;
using std::string;
using std::mem_fun;
using std::bind1st;

BadSampleFilter::
BadSampleFilter() :
    _filterBadSamples(false),
    _minDsmId(1),
    _maxDsmId(1024),
    _minSampleLength(1),
    _maxSampleLength(UINT_MAX),
    _minSampleTime(LONG_LONG_MIN),
    _maxSampleTime(LONG_LONG_MAX)
{
}

void
BadSampleFilter::
setFilterBadSamples(bool val)
{
    _filterBadSamples = val;
}

void
BadSampleFilter::
setMinDsmId(unsigned int val)
{
    _minDsmId = val;
    setFilterBadSamples(true);
}

void
BadSampleFilter::
setMaxDsmId(unsigned int val)
{
    _maxDsmId = val;
    setFilterBadSamples(true);
}

void
BadSampleFilter::
setMinSampleLength(unsigned int val)
{
    _minSampleLength = val;
    setFilterBadSamples(true);
}

void
BadSampleFilter::
setMaxSampleLength(unsigned int val)
{
    _maxSampleLength = val;
    setFilterBadSamples(true);
}

void
BadSampleFilter::
setMinSampleTime(const UTime& val)
{
    _minSampleTime = val.toUsecs();
    setFilterBadSamples(true);
}

void
BadSampleFilter::
setMaxSampleTime(const UTime& val)
{
    _maxSampleTime = val.toUsecs();
    setFilterBadSamples(true);
}


namespace {

    void
    convert(const string& text, bool& dest)
    {
        if (text == "on")
            dest = true;
        else if (text == "off")
            dest = false;
        else
        {
            throw NidasAppException("must be on or off: " + text);
        }
    }
        

    void
    convert(const string& text, unsigned int& nump)
    {
        unsigned int num;
        errno = 0;
        char* endptr;
        num = strtoul(text.c_str(), &endptr, 0);
        // We specifically prohibit negative numbers, since strtoul will
        // happily parse negatives and convert to unsigned.
        if (text.empty() || errno != 0 || *endptr != '\0' ||
            text.find('-') != string::npos)
        {
            throw NidasAppException("failed to parse: " + text);
        }
        nump = num;
    }

    void
    convert(const string& text, UTime& dtime)
    {
        try {
            dtime = UTime::parse(true, text);
        }
        catch (nidas::util::ParseException& ex)
        {
            throw NidasAppException("could not parse time from: " + text);
        }
    }

    template <typename T, typename F>
    inline bool
    check_rule(const string& name, const string& key,
               const string& value, const F& f)
    {
        if (key == name)
        {
            T setting;
            convert(value, setting);
            f(setting);
            return true;
        }
        return false;
    }

    inline bool
    check_time_rule(const string& name, const string& key,
                    const string& value,
                    BadSampleFilter* target,
                    void (BadSampleFilter::*f)(const UTime&))
    {
        if (key == name)
        {
            UTime setting;
            convert(value, setting);
            (target->*f)(setting);
            return true;
        }
        return false;
    }
}


void
BadSampleFilter::
setRule(const std::string& rule)
{
    if (rule.empty())
        return;

    string::size_type equal = rule.find('=');
    string field = rule;
    string value;
    if (rule == "on" || rule == "off")
    {
        value = rule;
    }
    else if (equal != string::npos)
    {
        field = rule.substr(0, equal);
        ++equal;
        value = rule.substr(equal, rule.length()-equal);
    }
    if (!value.empty() &&
        (check_rule<unsigned int>
         ("mindsm", field, value,
          bind1st(mem_fun(&BadSampleFilter::setMinDsmId), this)) ||
         check_rule<unsigned int>
         ("maxdsm", field, value,
          bind1st(mem_fun(&BadSampleFilter::setMaxDsmId), this)) ||
         check_rule<unsigned int>
         ("minlen", field, value,
          bind1st(mem_fun(&BadSampleFilter::setMinSampleLength), this)) ||
         check_rule<unsigned int>
         ("maxlen", field, value,
          bind1st(mem_fun(&BadSampleFilter::setMaxSampleLength), this)) ||
         check_time_rule
         ("mintime", field, value, this, &BadSampleFilter::setMinSampleTime) ||
         check_time_rule
         ("maxtime", field, value, this, &BadSampleFilter::setMaxSampleTime) ||
         check_rule<bool>
         ("on", field, value,
          bind1st(mem_fun(&BadSampleFilter::setFilterBadSamples), this)) ||
         check_rule<bool>
         ("off", field, value,
          bind1st(mem_fun(&BadSampleFilter::setFilterBadSamples), this))))
    {
        return;
    }
    throw NidasAppException("Filter rule must be on, off, or "
                            "<field>=<value>.  Could not parse: " + rule);
}


void
BadSampleFilter::
setRules(const string& fields)
{
    // An empty string is specifically allowed, but it has no effect.
    if (fields.empty())
        return;

    // We're parsing these fields into filter settings:
    // maxdsm,mindsm,maxtime,mintime,maxlen,minlen,on,off

    // Break the string at commas, then parse the fields as <field>=<value>.
    string::size_type at = 0;
    while (at < fields.length())
    {
        string::size_type comma = fields.find(',', at);
        if (comma == string::npos) 
            comma = fields.length();

        string field = fields.substr(at, comma-at);
        setRule(field);
        at = comma+1;
    }
}


bool
BadSampleFilter::
operator==(const BadSampleFilter& right) const
{
    return _filterBadSamples == right._filterBadSamples &&
        _minDsmId == right._minDsmId &&
        _maxDsmId == right._maxDsmId &&
        _minSampleLength == right._minSampleLength &&
        _maxSampleLength == right._maxSampleLength &&
        _minSampleTime == right._minSampleTime &&
        _maxSampleTime == right._maxSampleTime;
}


namespace nidas { namespace core {

std::ostream&
operator<<(std::ostream& out, const BadSampleFilter& bsf)
{
    out << (bsf.filterBadSamples() ? "on" : "off") << ","
        << "mindsm=" << bsf.minDsmId() << ","
        << "maxdsm=" << bsf.maxDsmId() << ","
        << "minlen=" << bsf.minSampleLength() << ","
        << "maxlen=" << bsf.maxSampleLength();
    if (bsf.minSampleTime().toUsecs() != LONG_LONG_MIN)
        out << "," << "mintime="
            << bsf.minSampleTime().format(true, "%Y-%m-%dT%H:%M:%S.%f");
    if (bsf.maxSampleTime().toUsecs() != LONG_LONG_MAX)
        out << "," << "maxtime="
            << bsf.maxSampleTime().format(true, "%Y-%m-%dT%H:%M:%S.%f");
    return out;
}

} }


BadSampleFilterArg::
BadSampleFilterArg() :
    NidasAppArg
  ("-f,--filter", "<rules>",
   "-f does not take any arguments.  It enables bad sample filtering\n"
   "  using the default range filters:\n"
   "    sample type must be valid\n"
   "    sample length must be greater than 0\n"
   "    dsmid must be greater than 0 and <= 1024\n"
   "    timetag must be within the last 20 years of tomorrow\n"
   "  Some programs use the start and end time to narrow the valid timetag range.\n"
   "--filter <rules>\n",
   "  Enable sample filtering and modify the valid ranges according to <rules>.\n"
   "  Rules are a comma-separated list of key=value settings using these keys:\n"
   "    maxdsm,mindsm,maxtime,mintime,maxlen,minlen\n"
   "  Any of the keys may be specified in any order.  Here is the default rule:\n"
   "    maxdsm=1024,mindsm=1,maxlen=32768,minlen=1\n"
   "If any sample fails any criteria, assume the sample header is corrupt\n"
   "and scan ahead for a good header.  Use only on corrupt data files."),
    _bsf()
{}


bool
BadSampleFilterArg::
parse(const ArgVector& argv, int* argi)
{
    bool result = false;
    int i = 0;
    if (argi)
        i = *argi;
    string flag = argv[i];
    if (accept(flag))
    {
        _arg = flag;
        // Only consume a value if this is not the short flag.
        if (flag != "-f")
        {
            _value = expectArg(argv, ++i);
            _bsf.setRules(_value);
        }
        result = true;
    }
    if (argi)
        *argi = i;
    return result;
}

