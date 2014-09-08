

#include "NidasApp.h"

#include <unistd.h>

using std::string;

using namespace nidas::core;
namespace n_u = nidas::util;

SampleMatcher::
RangeMatcher::
RangeMatcher(int d1, int d2, int s1, int s2, int inc) :
  dsm1(d1), dsm2(d2), sid1(s1), sid2(s2), include(inc)
{}


SampleMatcher::
RangeMatcher::
RangeMatcher() :
  dsm1(-1), dsm2(-1), sid1(-1), sid2(-1), include(true)
{}


SampleMatcher::
SampleMatcher() :
  _ranges(),
  _lookup()
{
}

bool
SampleMatcher::
addCriteria(const std::string& ctext)
{
  int dsmid1,dsmid2;
  int snsid1,snsid2;
  string soptarg(ctext);
  string::size_type ic = soptarg.find(',');
  if (ic == string::npos) 
    return false;
  string dsmstr = soptarg.substr(0,ic);
  string snsstr = soptarg.substr(ic+1);
  if (dsmstr.length() > 1 && (ic = dsmstr.find('-',1)) != string::npos) {
    dsmid1 = strtol(dsmstr.substr(0,ic).c_str(),0,0);
    dsmid2 = strtol(dsmstr.substr(ic+1).c_str(),0,0);
  }
  else {
    dsmid1 = dsmid2 = atoi(dsmstr.c_str());
  }
  if (snsstr.length() > 1 && (ic = snsstr.find('-',1)) != string::npos) {
    // strtol handles hex in the form 0xXXXX
    snsid1 = strtol(snsstr.substr(0,ic).c_str(),0,0);
    snsid2 = strtol(snsstr.substr(ic+1).c_str(),0,0);
  }
  else {
    snsid1 = snsid2 = strtol(snsstr.c_str(),0,0);
  }

#ifdef notdef
  if (snsstr.find("0x",0) != string::npos) 
    _idFormat = DumpClient::HEX_ID;
#endif

  _ranges.push_back(RangeMatcher(dsmid1, dsmid2, snsid1, snsid2, true));
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
  range_matches_t::iterator ri;
  for (ri = _ranges.begin(); ri != _ranges.end(); ++ri)
  {
    // See if the id is in this range, in which case the result depends
    // upon whether this range is included or excluded.
    int did = GET_DSM_ID(id);
    int sid = GET_SHORT_ID(id);
    if ((ri->dsm1 == -1 || (did >= ri->dsm1 && did <= ri->dsm2)) && 
	(ri->sid1 == -1 || (sid >= ri->sid1 && sid <= ri->sid2)))
    {
      _lookup[id] = ri->include;
      return ri->include;
    }
  }
  _lookup[id] = false;
  return false;
}


NidasApp::
NidasApp() :
  _allowedArguments(NoArgument),
  _logLevel(n_u::LOGGER_INFO),
  _processData(false),
  _xmlFileName(),
  _idFormat(DECIMAL),
  _sampleMatcher()
{
}


void
NidasApp::
setArguments(unsigned int mask)
{
  _allowedArguments = mask;
}

void
NidasApp::
parseArguments(const char** argv, int argc)
{
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, (char* const*)argv, "x:l:i:")) != -1)
    {
      switch (opt_char) {
      case 'x':
	_xmlFileName = optarg;
	break;
      case 'i':
	_sampleMatcher.addCriteria(optarg);
	break;
      case 'l':
	_logLevel = nidas::util::stringToLogLevel(optarg);
	break;
      }
    }

}


