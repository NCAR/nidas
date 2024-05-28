
#include "SampleMatcher.h"

#include <stdexcept>

using namespace nidas::core;
using std::string;


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
      throw std::invalid_argument(text);
    }
    return num;
  }
}


void
SampleMatcher::RangeMatcher::
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
    rngid1 = rngid2 = MATCH_ALL;
  }
  else if (rngstr == ".")
  {
    rngid1 = rngid2 = MATCH_FIRST;
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


bool
SampleMatcher::RangeMatcher::
parse_specifier(const std::string& specifier)
{
  // start out assuming this will include samples
  include = true;

  std::string spec = specifier;
  if (spec.empty())
    return false;
  if (spec[0] == '^')
  {
    include = false;
    spec = spec.substr(1);
  }
  string::size_type ic = spec.find(',');
  string dsmstr = spec.substr(0,ic);
  string snsstr;
  if (ic != string::npos)
    snsstr = spec.substr(ic+1);

  try
  {
    parse_range(dsmstr, dsm1, dsm2);
    if (!snsstr.empty())
      parse_range(snsstr, sid1, sid2);
    else
      sid1 = sid2 = MATCH_ALL;

    if (sid1 == MATCH_FIRST)
    {
      throw std::invalid_argument("invalid use of . to match sample id");
    }
  }
  catch (std::invalid_argument& err)
  {
    return false;
  }
  return true;
}


bool
SampleMatcher::RangeMatcher::
match(int dsmid, int sid)
{
  if (sid1 < 0 || (sid >= sid1 && sid <= sid2))
  {
    if (dsm1 == MATCH_FIRST)
    {
      dsm1 = dsm2 = dsmid;
    }
    if (dsm1 == MATCH_ALL || (dsmid >= dsm1 && dsmid <= dsm2))
    {
      return true;
    }
  }
  return false;
}


void
SampleMatcher::RangeMatcher::
set_first_dsm(int dsmid)
{
  if (dsm1 == MATCH_FIRST)
    dsm1 = dsm2 = dsmid;
}


SampleMatcher::
SampleMatcher() :
  _ranges(),
  _lookup(),
  _startTime(LONG_LONG_MIN),
  _endTime(LONG_LONG_MAX),
  _first_dsmid(0)
{
}


bool
SampleMatcher::
addCriteria(const std::string& ctext)
{
  // An empty criteria is allowed but changes nothing.
  if (ctext.empty())
  {
    return true;
  }

  RangeMatcher rm;
  bool valid = rm.parse_specifier(ctext);
  if (valid)
  {
    // invalidate the cache
    _lookup.clear();
    if (_first_dsmid)
      rm.set_first_dsm(_first_dsmid);
    _ranges.push_back(std::move(rm));
  }
  return valid;
}


bool
SampleMatcher::
match(dsm_sample_id_t id)
{
  id_lookup_t::iterator it = _lookup.find(id);
  if (it != _lookup.end())
  {
    return it->second;
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

  bool all_excludes = true;
  range_matches_t::iterator ri;
  for (ri = _ranges.begin(); ri != _ranges.end(); ++ri)
  {
    all_excludes = all_excludes && (!ri->include);
    // See if the id is in this range, in which case the result depends
    // upon whether this range is included or excluded.
    if (ri->match(did, sid))
      break;
  }

  // If there are no ranges, then the sample is implicitly included.  If
  // there are only excluded ranges and this sample did not match any of
  // them, then the sample is implicitly included.
  bool result = false;
  if (_ranges.begin() == _ranges.end())
    result = true;
  else if (ri != _ranges.end())
    result = ri->include;
  else if (all_excludes)
    result = true;
  _lookup[id] = result;
  return result;
}


bool
SampleMatcher::
match(const Sample* samp)
{
  dsm_time_t tt = samp->getTimeTag();
  dsm_sample_id_t sampid = samp->getId();
  if (tt < _startTime.toUsecs() || tt > _endTime.toUsecs() || !match(sampid))
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
