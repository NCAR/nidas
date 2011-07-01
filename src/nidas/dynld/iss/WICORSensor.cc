/*
 * WICORSensor.cpp
 *
 *  Created on: Jun 29, 2011
 *      Author: granger
 */

#include "WICORSensor.h"

#include <sstream>

// POSIX regex
#include <sys/types.h>
#include <regex.h>

using namespace nidas::dynld::iss;
using namespace std;

#include <nidas/util/Logger.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/Variable.h>

using nidas::core::SampleTag;
using nidas::core::Sample;
using nidas::core::getSample;
using nidas::core::VariableConverter;
using nidas::core::SampleT;
using nidas::core::floatNAN;
using nidas::util::InvalidParameterException;
using nidas::core::Variable;
using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION_NS(iss,WICORSensor)

namespace nidas
{

namespace dynld
{

namespace iss
{

WICORSensor::WICORSensor()
{
}

WICORSensor::~WICORSensor()
{
}

void
WICORSensor::addSampleTag(SampleTag* stag) throw (InvalidParameterException)
{
  if (getSampleTags().size() > 0)
  {
    throw InvalidParameterException(
        getName() + " can only create one sample for WICOR");
  }

  // size_t nvars = stag->getVariables().size();
  nidas::core::VariableIterator vi = stag->getVariableIterator();

  // For each variable, look for the regex parameter.
  for (int i = 0; vi.hasNext(); ++i)
  {
    const Variable* var = vi.next();
    const nidas::core::Parameter* pregex = var->getParameter("regex");
    std::string regex = pregex->getStringValue(0);
    DLOG(("found variable ") << var->getName() << ", regex=" << regex);
    _patterns.push_back(regex);
  }
  DLOG(("npatterns=") << _patterns.size());
  UDPSocketSensor::addSampleTag(stag);
}

bool
WICORSensor::process(const Sample* samp, std::list<const Sample*>& results)
  throw ()
{
  // WICOR messages embed field IDs in the message, so we can use regular expressions
  // to extract some subset of the fields without requiring a long and inflexible scanf.
  // Also, WICOR messages include the sample timestamp, so this method extracts that.
  assert(samp->getType() == nidas::core::CHAR_ST);

  if (_patterns.empty())
  {
    return false;
  }

  std::list<const SampleTag *> sample_tags = getSampleTags();
  if (sample_tags.begin() == sample_tags.end())
  {
    return false;
  }
  const SampleTag* stag = *sample_tags.begin();
  const char* inputstr = (const char*) samp->getConstVoidDataPtr();
  int slen = samp->getDataByteLength();

  // if sample is not null terminated, create a new null-terminated sample
  if (inputstr[slen - 1] != '\0')
  {
    SampleT<char>* newsamp = getSample<char> (slen + 1);
    newsamp->setTimeTag(samp->getTimeTag());
    newsamp->setId(samp->getId());
    char* newstr = (char*) newsamp->getConstVoidDataPtr();
    ::memcpy(newstr, inputstr, slen);
    newstr[slen] = '\0';

    bool res = WICORSensor::process(newsamp, results);
    newsamp->freeReference();
    return res;
  }

  SampleT<float>* outs = getSample<float> (_patterns.size());
  float* fp = outs->getDataPtr();

  int nparsed = 0;
  DLOG(("scanning: ") << inputstr);
  for (unsigned int i = 0; i < _patterns.size(); ++i)
  {
    regex_t compiled_regex;
    regmatch_t match;
    regcomp(&compiled_regex, _patterns[i].c_str(), REG_EXTENDED);
    // Never expect more than one match.
    if (0 == regexec(&compiled_regex, inputstr, 1, &match, 0) && match.rm_so >= 0)
    {
      std::string sub(inputstr + match.rm_so, inputstr + match.rm_eo);
      fp[i] = atof(sub.c_str());
      DLOG(("pattern ") << _patterns[i] << " retrieved " << fp[i]);
      ++nparsed;
    }
    else
    {
      ELOG(("could not match pattern ") << _patterns[i]);
      fp[i] = floatNAN;
    }
    regfree(&compiled_regex);
  }

  if (!nparsed)
  {
    outs->freeReference();
    return false; // no sample
  }

  const vector<const Variable*>& vars = stag->getVariables();
  int nd = 0;
  for (unsigned int iv = 0; iv < vars.size(); iv++)
  {
    const Variable* var = vars[iv];
    for (unsigned int id = 0; id < var->getLength(); id++, nd++, fp++)
    {
      if (nd >= nparsed || *fp == var->getMissingValue())
      {
        *fp = floatNAN;
      }
      else if (*fp < var->getMinValue() || *fp > var->getMaxValue())
      {
        *fp = floatNAN;
      }
      else if (getApplyVariableConversions())
      {
        VariableConverter* conv = var->getConverter();
        if (conv)
          *fp = conv->convert(samp->getTimeTag(), *fp);
      }
    }
  }
  outs->setTimeTag(samp->getTimeTag());
  outs->setDataLength(nd);
  results.push_back(outs);
  return true;
}

}
}

}

