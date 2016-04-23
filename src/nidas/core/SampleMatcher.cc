
#include "SampleMatcher.h"

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


  void
  parse_range(const std::string& rngstr, int& rngid1, int& rngid2)
  {
    string::size_type ic;
    if (rngstr.length() > 1 && (ic = rngstr.find('-',1)) != string::npos)
    {
      rngid1 = int_from_string(rngstr.substr(0,ic));
      rngid2 = int_from_string(rngstr.substr(ic+1));
    }
    else if (rngstr == "*")
    {
      rngid1 = rngid2 = -1;
    }
    else
    {
      rngid1 = rngid2 = int_from_string(rngstr);
    }
  }
}


SampleMatcher::
RangeMatcher::
RangeMatcher(int d1, int d2, int s1, int s2, int inc) :
  dsm1(d1), dsm2(d2), sid1(s1), sid2(s2), include(inc)
{}


SampleMatcher::
SampleMatcher() :
  _ranges(),
  _lookup(),
  _startTime(LONG_LONG_MIN),
  _endTime(LONG_LONG_MAX)
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
  bool include = true;
  int dsmid1,dsmid2;
  int snsid1,snsid2;
  string soptarg(ctext);
  if (soptarg.empty())
    return false;
  if (soptarg[0] == '^')
  {
    include = false;
    soptarg = soptarg.substr(1);
  }
  string::size_type ic = soptarg.find(',');
  if (ic == string::npos) 
    return false;
  string dsmstr = soptarg.substr(0,ic);
  string snsstr = soptarg.substr(ic+1);
  try
  {
    parse_range(dsmstr, dsmid1, dsmid2);
    parse_range(snsstr, snsid1, snsid2);
  }
  catch (std::invalid_argument& err)
  {
    return false;
  }
  _lookup.clear();
  _ranges.push_back(RangeMatcher(dsmid1, dsmid2, snsid1, snsid2, include));
  return true;
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
  bool all_excludes = true;
  range_matches_t::iterator ri;
  for (ri = _ranges.begin(); ri != _ranges.end(); ++ri)
  {
    all_excludes = all_excludes && (!ri->include);
    // See if the id is in this range, in which case the result depends
    // upon whether this range is included or excluded.
    int did = GET_DSM_ID(id);
    int sid = GET_SHORT_ID(id);
    if ((ri->dsm1 == -1 || (did >= ri->dsm1 && did <= ri->dsm2)) && 
	(ri->sid1 == -1 || (sid >= ri->sid1 && sid <= ri->sid2)))
    {
      break;
    }
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


