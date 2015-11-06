// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
/*

    Created on: Jun 29, 2011
        Author: granger

*/

#include "WICORSensor.h"

#include <sstream>

using namespace nidas::dynld::iss;
using namespace std;

#include <nidas/util/Logger.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/Variable.h>
#include <nidas/util/UTime.h>
#include <nidas/util/ParseException.h>

using nidas::util::UTime;
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

namespace nidas { namespace dynld { namespace iss {

WICORSensor::WICORSensor(): _patterns(),_regex(0)
{
}

WICORSensor::~WICORSensor()
{
    if (_regex) {
        for (unsigned int i = 0; i < _patterns.size(); ++i)
        {
            regfree(&_regex[i]);
        }
        delete[] _regex;
    }
}

namespace
{
    std::string match_float = "-?[0-9]*\\.?[0-9]*";
    std::string match_int = "-?[0-9]+";
}

void
    WICORSensor::addSampleTag(SampleTag* stag) throw (InvalidParameterException)
    {
        if (!getSampleTags().empty())
        {
            throw InvalidParameterException(
                    getName() + " can only create one sample for WICOR");
        }

        size_t nvars = stag->getVariables().size();
        if (nvars == 0)
        {
            return;
        }
        _regex = new regex_t[nvars];
        nidas::core::VariableIterator vi = stag->getVariableIterator();

        // For each variable, look for the regex parameter.
        unsigned int i;
        for (i = 0; vi.hasNext(); ++i)
        {
            const Variable* var = vi.next();
            const nidas::core::Parameter* pregex = var->getParameter("regex");
            std::string regex_value = pregex->getStringValue(0);
            std::string regex = regex_value;

            // Replace all occurrences of %f rather than just the first, in case
            // other numbers need to be matched to locate the desired number.
            std::string::size_type pos = regex.find("%f");
            while (pos != std::string::npos)
            {
                regex.replace(pos, 2, match_float);
                pos = regex.find("%f");
            }
            DLOG(("variable ") << var->getName() << ": regex=" << regex_value
                    << ", expanded=" << regex);

            regex_t* preg = _regex + i;
            int errcode = regcomp(preg, regex.c_str(), REG_EXTENDED);
            if (errcode != 0)
            {
                char errbuf[256];
                regerror(errcode, preg, errbuf, sizeof(errbuf));
                throw InvalidParameterException(
                        getName() + " regex compile error for pattern '" + regex +
                        "': " + errbuf);
            }
            _patterns.push_back(regex);
        }
        UDPSocketSensor::addSampleTag(stag);
    }

bool
    WICORSensor::process(const Sample* samp, std::list<const Sample*>& results)
    throw ()
    {
        // WICOR messages embed field IDs in the message, so we can use regular
        // expressions to extract some subset of the fields without requiring a
        // long and inflexible scanf.  Also, WICOR messages include the sample
        // timestamp, so this method extracts that.
        assert(samp->getType() == nidas::core::CHAR_ST);

        if (_patterns.empty())
        {
            return false;
        }

        list<SampleTag *>& sample_tags = getSampleTags();
        if (sample_tags.begin() == sample_tags.end())
        {
            return false;
        }
        SampleTag* stag = *sample_tags.begin();
        const char* inputstr = (const char*) samp->getConstVoidDataPtr();
        int slen = samp->getDataByteLength();

        // if sample is not null terminated, create a new null-terminated sample
        if (inputstr[slen - 1] != '\0')
        {
            DLOG(("creating a new null-terminated sample"));
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

        DLOG(("scanning: ") << inputstr);

        // Try to extract the time.
        UTime ut;
        try
        {
            int nparsed = 0;
            ut = UTime::parse(true, inputstr, "$WICOR,%d%m%y,%H%M%S", &nparsed);
            if (nparsed < 20)
            {
                ELOG(("failed to parse time: ") << inputstr);
                return false;
            }
        }
        catch (const nidas::util::ParseException&)
        {
            ELOG(("exception parsing time: ") << inputstr);
            return false;
        }

        SampleT<float>* outs = getSample<float> (_patterns.size());
        float* fp = outs->getDataPtr();
        outs->setTimeTag(ut.toUsecs());
        outs->setId(stag->getId());
        DLOG(("created sample id=")
                << outs->getId() << ", time tag="
                << UTime(outs->getTimeTag()).format());

        int nparsed = 0;
        for (unsigned int i = 0; i < _patterns.size(); ++i)
        {
            regmatch_t matches[2];
            // The first match is the entire matching substring, so we use the
            // second group, if present, to get the subgroup match.  If there is
            // only the matching substring, then use it, but in that case the
            // number to retrieve needs to be in front.
            if (0 == regexec(_regex + i, inputstr, 2, matches, 0))
            {
                regmatch_t* pmatch = &(matches[1]);
                if (pmatch->rm_so == -1)
                {
                    --pmatch;
                }
                if (pmatch->rm_so >= 0)
                {
                    std::string sub(inputstr + pmatch->rm_so, inputstr + pmatch->rm_eo);
                    fp[i] = atof(sub.c_str());
                    DLOG(("pattern ") << _patterns[i]
                            << " matched '" << sub << "' and retrieved " << fp[i]);
                    ++nparsed;
                }
            }
            else
            {
                ELOG(("could not match pattern ") << _patterns[i]);
                fp[i] = floatNAN;
            }
        }

        if (!nparsed)
        {
            outs->freeReference();
            return false; // no sample
        }

        const vector<Variable*>& vars = stag->getVariables();
        int nd = 0;
        // Not all variables may have parsed correctly, but at least one
        // parsed, and any not parsed will have been assigned NaN, so all
        // values will go into the sample.
        for (unsigned int iv = 0; iv < vars.size(); iv++)
        {
            Variable* var = vars[iv];
            for (unsigned int id = 0; id < var->getLength(); id++, nd++, fp++)
            {
                if (*fp == var->getMissingValue())
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
        outs->setDataLength(nd);
        results.push_back(outs);
        return true;
    }

}
}

}

