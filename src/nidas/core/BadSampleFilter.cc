// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:

#include "BadSampleFilter.h"
#include <functional>

using nidas::core::BadSampleFilter;
using nidas::core::BadSampleFilterArg;
using nidas::util::UTime;
using nidas::core::NidasAppException;
using nidas::util::LogContext;
using nidas::util::LogMessage;
using std::string;
using std::mem_fn;

BadSampleFilter::
BadSampleFilter() :
    _filterBadSamples(false),
    _minDsmId(1),
    _maxDsmId(1024),
    _minSampleLength(1),
    _maxSampleLength(32768),
    _minSampleTime(LONG_LONG_MIN),
    _maxSampleTime(LONG_LONG_MAX),
    _skipNidasHeader(false),
    _sampleType(UNKNOWN_ST)
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

void
BadSampleFilter::
setSkipNidasHeader(bool enable)
{
    _skipNidasHeader = enable;
}

void
BadSampleFilter::
setSampleTypeLimit(nidas::core::sampleType stype)
{
    _sampleType = stype;
    setFilterBadSamples(true);
}

void
BadSampleFilter::
setDefaultTimeRange(const UTime& start, const UTime& end)
{
    // Set the start and end times as filter times only if unset,
    // and do not change whether the filter is enabled.
    if (_minSampleTime == LONG_LONG_MIN && start.isSet())
    {
        _minSampleTime = start.toUsecs() - USECS_PER_DAY;
    }
    if (_maxSampleTime == LONG_LONG_MAX && end.isSet())
    {
        _maxSampleTime = end.toUsecs() + USECS_PER_DAY;
    }
}


bool
BadSampleFilter::
invalidSampleHeader(const SampleHeader& sheader)
{
    static LogContext lp(LOG_VERBOSE);

    if (!_filterBadSamples)
        return false;

    // screen bad headers.
    bool bad =
        ((_sampleType != UNKNOWN_ST && sheader.getType() != _sampleType) ||
         sheader.getType() >= UNKNOWN_ST ||
         GET_DSM_ID(sheader.getId()) < _minDsmId ||
         GET_DSM_ID(sheader.getId()) > _maxDsmId ||
         sheader.getDataByteLength() < _minSampleLength ||
         sheader.getDataByteLength() > _maxSampleLength ||
         sheader.getTimeTag() < _minSampleTime ||
         sheader.getTimeTag() > _maxSampleTime);

    // if verbose logging enabled, provide details on why the header is
    // invalid.
    if (bad && lp.active())
    {
        lp.log() << explainFilter(sheader);
    }
    return bad;
}


std::string
BadSampleFilter::
explainFilter(const SampleHeader& sheader)
{
    std::ostringstream msg;
    msg << "invalid sample header fields: ";
    if (_sampleType != UNKNOWN_ST && sheader.getType() != _sampleType)
        msg << "header type not accepted: " << (int)sheader.getType() << "; ";
    if (sheader.getType() >= UNKNOWN_ST)
        msg << "header type invalid: " << (int)sheader.getType() << "; ";
    if (GET_DSM_ID(sheader.getId()) < _minDsmId || GET_DSM_ID(sheader.getId()) > _maxDsmId)
        msg << "dsmid out of range: " << GET_DSM_ID(sheader.getId()) << "; ";
    if (sheader.getDataByteLength() < _minSampleLength || sheader.getDataByteLength() > _maxSampleLength)
        msg << "datalen out of range: " << sheader.getDataByteLength() << "; ";
    if (sheader.getTimeTag() < _minSampleTime || sheader.getTimeTag() > _maxSampleTime)
    {
        UTime tt(sheader.getTimeTag());
        msg << "time out of range: "
            << tt.format(true, "%Y-%m-%dT%H:%M:%S.%f") << ";";
    }
    return msg.str();
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
               const string& value, const F& f, BadSampleFilter* bsf)
    {
        if (key == name)
        {
            T setting;
            convert(value, setting);
            f(bsf, setting);
            return true;
        }
        return false;
    }

    inline bool
    check_time_rule(const string& name, const string& key,
                    const string& value,
                    void (BadSampleFilter::*f)(const UTime&),
                    BadSampleFilter* target)
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
    if (rule == "on" || rule == "off" || rule == "raw")
    {
        value = rule;
    }
    else if (equal != string::npos)
    {
        field = rule.substr(0, equal);
        ++equal;
        value = rule.substr(equal, rule.length()-equal);
    }
    if (value == "raw")
    {
        setSampleTypeLimit(CHAR_ST);
    }
    else if (!value.empty() &&
        (check_rule<unsigned int>
         ("mindsm", field, value,
          mem_fn(&BadSampleFilter::setMinDsmId), this) ||
         check_rule<unsigned int>
         ("maxdsm", field, value,
          mem_fn(&BadSampleFilter::setMaxDsmId), this) ||
         check_rule<unsigned int>
         ("minlen", field, value,
          mem_fn(&BadSampleFilter::setMinSampleLength), this) ||
         check_rule<unsigned int>
         ("maxlen", field, value,
          mem_fn(&BadSampleFilter::setMaxSampleLength), this) ||
         check_time_rule
         ("mintime", field, value, &BadSampleFilter::setMinSampleTime, this) ||
         check_time_rule
         ("maxtime", field, value, &BadSampleFilter::setMaxSampleTime, this) ||
         check_rule<bool>
         ("skipnidasheader", field, value,
          mem_fn(&BadSampleFilter::setSkipNidasHeader), this) ||
         check_rule<bool>
         ("on", field, value,
          mem_fn(&BadSampleFilter::setFilterBadSamples), this) ||
         check_rule<bool>
         ("off", field, value,
          mem_fn(&BadSampleFilter::setFilterBadSamples), this)))
    {
        // all good.
    }
    else
    {
        throw NidasAppException("Filter rule must be on, off, raw or "
                                "<field>=<value>.  Could not parse: " + rule);
    }
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
        _maxSampleTime == right._maxSampleTime &&
        _skipNidasHeader == right._skipNidasHeader &&
        _sampleType == right._sampleType;
}


namespace nidas { namespace core {

std::ostream&
operator<<(std::ostream& out, const BadSampleFilter& bsf)
{
    out << (bsf.filterBadSamples() ? "on" : "off") << ","
        << "skipnidasheader="
        << (bsf.skipNidasHeader() ? "on" : "off") << ","
        << "mindsm=" << bsf.minDsmId() << ","
        << "maxdsm=" << bsf.maxDsmId() << ","
        << "minlen=" << bsf.minSampleLength() << ","
        << "maxlen=" << bsf.maxSampleLength();
    if (bsf.sampleTypeLimit() != UNKNOWN_ST)
        out << "," << "type=" << (int)bsf.sampleTypeLimit();
    else
        out << "," << "type=valid";
    if (bsf.minSampleTime().isSet())
        out << "," << "mintime="
            << bsf.minSampleTime().format(true, "%Y-%m-%dT%H:%M:%S.%f");
    if (bsf.maxSampleTime().isSet())
        out << "," << "maxtime="
            << bsf.maxSampleTime().format(true, "%Y-%m-%dT%H:%M:%S.%f");
    return out;
}

} }


BadSampleFilterArg::
BadSampleFilterArg() :
    NidasAppArg
  ("-f,--filter", "<rules>",
   "-f alone for default filters, --filter to set rules.\n"
   "  Here are the default filter rules:\n"
   "    sample type must be valid\n"
   "    sample length must be greater than 0\n"
   "    dsmid must be greater than 0 and <= 1024\n"
   "    timetag must be within the last 20 years of tomorrow\n"
   "  Some programs use start and end time to narrow the valid timetag range.\n"
   "--filter <rules>\n"
   "  Enable sample filtering and modify the valid ranges according to <rules>.\n"
   "  Rules are a comma-separated list of key=value settings using these keys:\n"
   "    skipnidasheader,raw,maxdsm,mindsm,maxtime,mintime,maxlen,minlen\n"
   "  Any of the keys may be specified in any order.  Here is the default rule:\n"
   "    skipnidasheader=off,maxdsm=1024,mindsm=1,maxlen=32768,minlen=1\n"
   "  Specify raw to accept only char sample types, like '--filter raw'.\n"
   "  Samples will always be rejected if the length is not a multiple of the\n"
   "  size of the sample type.\n"
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
        else
        {
            _bsf.setFilterBadSamples(true);
        }
        result = true;
    }
    if (argi)
        *argi = i;
    return result;
}

