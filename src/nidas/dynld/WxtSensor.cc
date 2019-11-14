// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#include "WxtSensor.h"
#include <nidas/core/AsciiSscanf.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Variable.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

#include <nidas/util/Logger.h>

using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION(WxtSensor)

#include "string_token.h"
#include <cstdio>

WxtSensor::WxtSensor():
    _field_formats(),
    _uName("U"),_vName("V"),
    _speedIndex(-1),_dirIndex(-1),_uIndex(-1),_vIndex(-1),
    _speedDirId(0),_uvId(0),_uvlen(0)
{
}

WxtSensor::~WxtSensor()
{
}

void WxtSensor::init() throw(nidas::util::InvalidParameterException)
{
    CharacterSensor::init();

    static struct ParamSet {
        const char* name;	// parameter name
        void (WxtSensor::* setFunc)(const string&);
        			// ptr to setXXX member function
				// for setting parameter.
    } paramSet[] = {
	{ "u",			&WxtSensor::setUName },
	{ "v",			&WxtSensor::setVName },
    };

    // Scan the parameters for this sensor, looking for those
    // with the above names. The value of those parameters
    // is the expected initial portion of the variable name
    // for the given quantity.
    for (unsigned int i = 0; i < sizeof(paramSet) / sizeof(paramSet[0]); i++) {
	const Parameter* param = getParameter(paramSet[i].name);
	if (!param) continue;
	if (param->getLength() != 1) 
	    throw nidas::util::InvalidParameterException(getName(),
		"parameter", string("bad length for ") + paramSet[i].name);
	// invoke setXXX member function
	(this->*paramSet[i].setFunc)(param->getStringValue(0));
    }

    unsigned int unl = getUName().length();
    unsigned int vnl = getVName().length();

    for (SampleTagIterator si = getSampleTagIterator(); si.hasNext(); ) {
	const SampleTag* stag = si.next();

        // check the initial characters in the variable names to determine
        // which is wind u and v.  Rather than impose a policy on those
        // variable names, the names can be specified by the user as
        // parameters, above.
        VariableIterator vi = stag->getVariableIterator();
        for (int i = 0; vi.hasNext(); i++) {
            const Variable* var = vi.next();
            const string& vname = var->getName();
            dsm_sample_id_t uvId = 0;

            if (vname == getUName() ||
                (vname.length() >= unl + 1 &&
                 vname.substr(0,unl+1) == getUName() + '.')) {
                _uIndex = i;
                uvId = stag->getId();
                _uvlen = stag->getVariables().size();
            }
            else if (vname == getVName() ||
                (vname.length() >= vnl + 1 &&
                 vname.substr(0,vnl+1) == getVName() + '.')) {
                _vIndex = i;
                uvId = stag->getId();
            }
            if (uvId != 0) {
                if (_uvId == 0) _uvId = uvId;
                else if (_uvId != uvId) 
                    throw nidas::util::InvalidParameterException(getName() +
                            " derived U and V must be in one sample");
            }
        }
    }

    const std::list<AsciiSscanf*>& sscanfers = getScanfers();
    std::list<AsciiSscanf*>::const_iterator si = sscanfers.begin();
    for ( ; si != sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
        const SampleTag* stag = sscanf->getSampleTag();

        // Tokenize the sscanf format into comma-separated fields.
        vector<string> field_formats;
        string_token(field_formats, sscanf->getFormat());

        // save fields for sample parsing.
        _field_formats[stag->getId()] = field_formats;

        vector<string>::iterator fi = field_formats.begin();
        int nfield = 0;
        for ( ; fi != field_formats.end(); ++fi) {
            if (fi->find("%f") != string::npos &&
                fi->rfind("%f") == fi->find("%f")) {

                if (fi->length() > 3) {
                    // look for Dm= field, which is wind direction mean
                    if (fi->substr(0,3) == "Dm=") {
                        _dirIndex = nfield;
                        _speedDirId = stag->getId();
                    }
                    // look for Sm= field, which is wind speed mean
                    else if (fi->substr(0,3) == "Sm=") {
                        _speedIndex = nfield;
                    }
                }
                nfield++;
            }
        }
        if (nfield != sscanf->getNumberOfFields())
        {
            throw nidas::util::InvalidParameterException
                (getName(), "scanfFormat",
                 "only %f conversions are supported for WxtSensor, "
                 "and only 1 per comma separated field. Use %*f to skip a field");
        }
    }
    if (_uvId != 0 && (_dirIndex < 0 || _speedIndex < 0)) {
        throw InvalidParameterException
            (getName(), "scanfFormat",
             "Sm and/or Dm fields are not found in scanfFormat. "
             "Cannot derive wind U and V from speed and direction");
    }
}


int WxtSensor::scanSample(AsciiSscanf* sscanf, const char* inputstr, float* dataptr)
{
    const SampleTag* stag = sscanf->getSampleTag();
    dsm_sample_id_t sid = stag->getId();
    const std::vector<const Variable*>& variables = stag->getVariables();
    vector<string>& field_formats = _field_formats[sid];

    int nparsed = 0;
    vector<string> sample_fields;
    string_token(sample_fields, inputstr);

    // Loop through the field formats in the scan string and their
    // corresponding variables, filling in variables as they match up with
    // a field in the sample.  If a variable does not match up, then assume
    // the WXT is not reporting that field and assign it a missing value.
    vector<string>::iterator fi = field_formats.begin();
    vector<string>::iterator si = sample_fields.begin();
    std::vector<const Variable*>::const_iterator vi = variables.begin();

    while (fi != field_formats.end() && vi != variables.end())
    {
        const Variable* var = *vi;
        // If we're out of sample fields, then the remaining variables get
        // missing values.
        if (si == sample_fields.end())
        {
            VLOG(("out of sample fields, filling variable with nan: ")
                 << var->getName());
	    *(dataptr++) = floatNAN;
            ++fi;
            ++vi;
        }

	// Exact match, so no specifiers, just keep looking for the next
	// variable field.
	if (*fi == *si)
	{
	    VLOG(("format match: ") << *fi << " == " << *si);
            ++fi;
            ++si;
	    continue;
	}

        // If it's the header, accept 1R0, 1r0, 0R0, and 0r0.
        const std::string& sf = *si;
        if (fi == field_formats.begin() &&
            fi->length() == si->length() &&
            fi->length() == 3 &&
            (sf[0] == '0' || sf[0] == '1') &&
            (std::toupper(sf[1]) == std::toupper((*fi)[1])) &&
            (sf[2] == (*fi)[2]))
        {
            VLOG(("loose match on WXT sample message header: ")
                 << sf << " =~ " << (*fi));
            ++fi;
            ++si;
	    continue;
        }
        else if (fi == field_formats.begin())
        {
            WLOG(("WXT sample message header did not match format: ")
                 << sf << " != " << (*fi));
            break;
        }

        // Make sure the sample field and the format field match up in the
        // first two characters, to make sure the fields are lined up.  If
        // not, this is the wrong sample field for this variable, so fill
        // it with nan and move on.
        float value = floatNAN;
        if (fi->substr(0, 2) == si->substr(0, 2))
        {
	    string full = (*fi) + "%n";
	    int n = 0;
	    if (std::sscanf(si->c_str(), full.c_str(), &value, &n) >= 1 &&
		(unsigned)n == si->length())
	    {
		++nparsed;
	    }
	    VLOG(("field scanned (") << *fi << "): " << *si << " = " << value);
            ++si;
	}
	else
	{
	    DLOG(("") << getName()
                 << ": format field "
                 << *fi << " does not match next sample field "
		 << *si
                 << ", setting variable " << var->getName() << " to nan");
	}
        *(dataptr++) = value;
        ++fi;
        ++vi;
    }
    // We need to return all or nothing, even if we only got something.
    // Even if not all the fields parsed and scanned correctly, the
    // returned data values should match up with each variables, where the
    // variable value is NaN if it did not scan.
    if (nparsed > 0)
	return sscanf->getNumberOfFields();
    return 0;
}

bool WxtSensor::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    std::list<const Sample*> vane;
    SerialSensor::process(samp,results);

    if (results.empty()) return false;
    if (_speedIndex < 0 || _dirIndex < 0 || _uIndex < 0 || _vIndex < 0)
    	return true;

    std::list<const Sample*>::const_iterator si = results.begin();
    for ( ; si != results.end(); ++si) {

        // result from base class parsing of ASCII, and correction of any cal file
        const Sample* csamp = *si;
        if (csamp->getId() != _speedDirId) continue;

        unsigned int slen = csamp->getDataLength();

        if ((signed) slen <= _speedIndex) continue;
        if ((signed) slen <= _dirIndex) continue;

        float spd = csamp->getDataValue(_speedIndex);
        float dir = csamp->getDataValue(_dirIndex);

        // derive U,V from Spd,Dir
        float u = -spd * ::sin(dir * M_PI / 180.0);
        float v = -spd * ::cos(dir * M_PI / 180.0);

        SampleT<float>* news = getSample<float>(_uvlen);
        news->setTimeTag(csamp->getTimeTag());
        news->setId(_uvId);

        float* nfptr = news->getDataPtr();
        nfptr[_uIndex] = u;
        nfptr[_vIndex] = v;

        results.push_back(news);
        break;
    }
    return true;
}
