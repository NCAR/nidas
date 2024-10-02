
#include "SampleMatcher.h"
#include <nidas/util/UTime.h>

#include <stdexcept>

using namespace nidas::core;
using std::string;
using std::invalid_argument;

using nidas::util::UTime;
using nidas::util::ParseException;


namespace 
{
  int
  int_from_string(const std::string& text)
  {
    int num;
    errno = 0;
    char* endptr;
    num = strtol(text.c_str(), &endptr, 0);
    if (text.empty() || errno != 0 || *endptr != '\0')
    {
      throw std::invalid_argument("cannot convert to int: " + text);
    }
    return num;
  }

  // Parse a time string, @throw ParseException if it fails
  // or if the whole string is not parsed.  If @p text is empty,
  // do not change @p when.
  void
  time_from_string(dsm_time_t& when, const std::string& text)
  {
    if (! text.empty())
    {
      when = UTime::convert(text).toUsecs();
    }
  }
}

const int RangeMatcher::MATCH_FIRST = -9;
const int RangeMatcher::MATCH_ALL = -1;
const dsm_time_t RangeMatcher::MATCH_ALL_TIME = LONG_LONG_MIN;


static void
parse_range(const std::string& rngstr, int& rngid1, int& rngid2)
{
  string::size_type ic;
  if (rngstr.length() > 1 && (ic = rngstr.find('-',1)) != string::npos)
  {
    rngid1 = int_from_string(rngstr.substr(0,ic));
    rngid2 = int_from_string(rngstr.substr(ic+1));
  }
  else if (rngstr == "*" || rngstr == "/")
  {
    rngid1 = rngid2 = RangeMatcher::MATCH_ALL;
  }
  else if (rngstr == ".")
  {
    rngid1 = rngid2 = RangeMatcher::MATCH_FIRST;
  }
  else
  {
    rngid1 = rngid2 = int_from_string(rngstr);
    if ((rngid1 < 0 && rngid1 != -1) || (rngid2 < 0 && rngid2 != -1))
    {
      throw std::invalid_argument("only negative id allowed is -1");
    }
  }
}


RangeMatcher&
RangeMatcher::
parse_specifier(const std::string& specifier)
{
  std::string spec = specifier;
  // start out assuming this will include samples
  include = true;
  if (!spec.empty() && spec[0] == '^')
  {
    include = false;
    spec = spec.substr(1);
  }

  // a very simple parser: always at least a dsm range, then maybe sid range.
  // if an sid range, it has to be in the second field.  a time range starts
  // with left bracket, and a file pattern starts with file=.
  try {
    int nfields = 0;
    string::size_type ic;
    do {
      if (++nfields > 4)
        throw invalid_argument("too many fields");

      // isolate the next field, possibly enclosed in brackets
      ic = spec.find(',');
      if (!spec.empty() && spec[0] == '[')
      {
        string::size_type b2 = spec.find(']');
        if (b2 == string::npos)
          throw invalid_argument("missing closing ]");
        // there must be a comma or nothing after the bracket
        ic = spec.find(',', ++b2);
        if (b2 < spec.length() && spec[b2] != ',')
          throw invalid_argument("missing comma after ]");
        // leave the brackets to identify the field as a time range
      }
      string field = spec.substr(0, ic);
      spec = (ic == string::npos ? "" : spec.substr(ic+1));

      if (field.empty())
      {
        throw invalid_argument("empty field not allowed");
      }
      if (nfields == 1)
      {
        parse_range(field, dsm1, dsm2);
      }
      else if (field[0] == '[')
      {
        // now clip the brackets
        field = field.substr(1, field.length()-2);
        string::size_type tsep = field.find(',');
        // comma separator is required to differentiate begin from end, even
        // though begin or end can be omitted.
        if (tsep == string::npos)
          throw invalid_argument("missing comma in time range");
        // these throw ParseException if the time strings do not parse
        time_from_string(time1, field.substr(0, tsep));
        time_from_string(time2, field.substr(tsep+1));
      }
      else if (field.substr(0,5) == "file=")
      {
        file_pattern = field.substr(5);
      }
      else if (nfields == 2)
      {
        parse_range(field, sid1, sid2);
        if (sid1 == MATCH_FIRST)
        {
          throw invalid_argument("invalid use of . to match sample id");
        }
      }
      else
      {
        throw invalid_argument("unrecognized field: " + field);
      }
      // ic points to the comma before the next field, or else there are no
      // more fields.
    } while (ic != string::npos);
  }
  catch (invalid_argument& e)
  {
    throw ParseException(e.what());
  }
  return *this;
}


bool
RangeMatcher::
match(int dsmid, int sid)
{
  if (sid < sid1 || (sid > sid2 && sid2 >= 0))
  {
    return false;
  }
  // set the first dsmid only once everything else matches
  if (dsm1 == MATCH_FIRST)
  {
    dsm1 = dsm2 = dsmid;
  }
  if (dsmid < dsm1 || (dsmid > dsm2 && dsm2 >= 0))
  {
    return false;
  }
  return true;
}


bool
RangeMatcher::
match_time(dsm_time_t stime)
{
  return (stime == MATCH_ALL_TIME) ||
    (stime >= time1 && (time2 == 0 || stime <= time2));
}


bool
RangeMatcher::
match_file(const std::string& filename)
{
  // find("") returns 0, so filename matches if filename is empty or
  // file_pattern is empty, otherwise filename must contain file_pattern.
  return file_pattern.empty() || filename.find(file_pattern) != string::npos;
}


void
RangeMatcher::
set_first_dsm(int dsmid)
{
  if (dsm1 == MATCH_FIRST)
    dsm1 = dsm2 = dsmid;
}


SampleMatcher::
SampleMatcher() :
  _ranges(),
  _lookup(),
  _startTime(UTime::MIN),
  _endTime(UTime::MAX),
  _first_dsmid(0),
  // this defaults to true because no ranges implies all samples are included,
  // same as if all ranges are excludes.
  _all_excludes(true)
{
  _lookup.clear();
}


void
SampleMatcher::
addCriteria(const std::string& ctext)
{
  // An empty criteria is allowed but changes nothing.
  if (! ctext.empty())
  {
    RangeMatcher rm;
    rm.parse_specifier(ctext);
    addCriteria(rm);
  }
}


void
SampleMatcher::
addCriteria(const RangeMatcher& rm)
{
  _ranges.push_back(rm);
  if (_first_dsmid)
    _ranges.back().set_first_dsm(_first_dsmid);

  // reset the cache.  if all ranges are excludes, then we can automatically
  // include any sample with an id not in the cache.
  _all_excludes = true;
  for (auto& rm: _ranges)
  {
    _all_excludes = _all_excludes && (!rm.include);
  }

  // as ids are matched, they are added to the lookup cache: true if the
  // id alone is matched by any of the range criteria, and otherwise false.
  // the next time an id is matched, if the cache value is false, then
  // the id is matched only if all_excludes is true.  if the cache value
  // is true, then all the ranges have to be checked to see if one
  // matches the id and also the time and filename criteria.
  _lookup.clear();
}


bool
SampleMatcher::
match(dsm_sample_id_t id, dsm_time_t tt, const std::string& filename)
{
  id_lookup_t::iterator it = _lookup.find(id);
  if (it != _lookup.end())
  {
    // if this id alone is not matched by of the ranges, then the other
    // criteria do not need to be checked.
    if (!it->second)
    {
      return _all_excludes;
    }
  }
  int did = GET_DSM_ID(id);
  int sid = GET_SHORT_ID(id);

  // it is feasible to defer this setting until a range first matches a
  // sample, and then fill in _just_ that range with the dsm id of that
  // sample, and in fact RangeMatcher still supports that.  and this code
  // could be separated into a method which the application has to call when
  // it needs all MATCH_FIRST references resolved immediately, ie, data_stats.
  // however, in the interest of having consistent sample matching behavior in
  // all apps, force all dsm ids to be resolved after the very first sample.
  // really this makes more sense for network streams anyway, so separate
  // sample id ranges all with MATCH_FIRST dsm will all match the same DSM.
  if (!_first_dsmid)
  {
    _first_dsmid = did;
    for (auto& rm: _ranges)
      rm.set_first_dsm(_first_dsmid);
  }

  // no choice but to find the range which matches the id as well as the
  // current time and filename.
  bool id_matched_range = false;
  range_matches_t::iterator ri;
  for (ri = _ranges.begin(); ri != _ranges.end(); ++ri)
  {
    if (ri->match(did, sid))
    {
      id_matched_range = true;
      if (ri->match_time(tt) && ri->match_file(filename))
        break;
    }
  }

  // If there are no ranges or only excluded ranges that the sample did not
  // match, then the sample is implicitly included, otherwise the sample is
  // included according to the matched range.
  bool result = (_all_excludes && ri == _ranges.end()) ||
                (ri != _ranges.end() && ri->include);
  // cache whether the id alone matched any ranges, since if it didn't there
  // is no point in checking the other criteria next time.
  _lookup[id] = id_matched_range;
  return result;
}


bool
SampleMatcher::
match(const Sample* samp, const std::string& inputname)
{
  dsm_time_t tt = samp->getTimeTag();
  dsm_sample_id_t sampid = samp->getId();
  if (tt < _startTime.toUsecs() || tt > _endTime.toUsecs() ||
      !match(sampid, tt, inputname))
  {
    return false;
  }
  return true;
}


bool
SampleMatcher::
exclusiveMatch()
{
  bool xc = false;
  if (_ranges.size() == 1)
  {
    range_matches_t::iterator ri = _ranges.begin();
    xc = ((ri->dsm1 == ri->dsm2 && ri->dsm1 != -1) &&
          (ri->sid1 == ri->sid2 && ri->sid1 != -1));
  }
  return xc;
}
