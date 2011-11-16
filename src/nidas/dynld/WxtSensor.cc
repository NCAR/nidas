// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ********************************************************************
 */

#include "WxtSensor.h"
#include <nidas/core/AsciiSscanf.h>

#include <sstream>

using namespace nidas::dynld;
using namespace std;

#include <nidas/util/Logger.h>

using nidas::util::LogContext;
using nidas::util::LogMessage;

NIDAS_CREATOR_FUNCTION(WxtSensor)

#include "string_token.h"
#include <cstdio>

WxtSensor::
WxtSensor()
{
}

WxtSensor::~WxtSensor()
{
}


int
WxtSensor::
scanSample(AsciiSscanf* sscanf, const char* inputstr, float* dataptr)
{
    int nparsed = 0;
    vector<string> sample_fields;
    string_token(sample_fields, inputstr);

    // Tokenize this format into comma-separated fields, and make sure
    // each matches in turn.
    vector<string> field_formats;
    string_token(field_formats, sscanf->getFormat());

    vector<string>::iterator fi = field_formats.begin();
    vector<string>::iterator si = sample_fields.begin();
    for ( ; fi != field_formats.end() && si != sample_fields.end() ; 
	  ++fi, ++si )
    {
	// Exact match, so no specifiers, just keep going
	if (*fi == *si)
	{
	    DLOG(("format match: ") << *fi << " == " << *si);
	    continue;
	}

	// There must be exactly one specifier in this field
	if (fi->find("%f") != string::npos &&
	    fi->rfind("%f") == fi->find("%f"))
	{
	    float value;
	    string full = (*fi) + "%n";
	    int n = 0;
	    if (std::sscanf(si->c_str(), full.c_str(), &value, &n) < 1 ||
		(unsigned)n != si->length())
	    {
		value = floatNAN;
	    }
	    else
	    {
		++nparsed;
	    }
	    *(dataptr++) = value;
	    DLOG(("field scanned (") << *fi << "): " << *si << " = " << value);
	}
	else
	{
	    // The only other possibility is a broken format or a mismatch
	    // in some expected verbatim match, in which case we fail.  We
	    // don't try to continue, because we can get out of sync with
	    // which variables this format was supposed to match.
	    ELOG(("invalid format or field mismatch: ") 
		 << *si << " does not scan with " << *fi);
	    break;
	}
    }
    // We need to return all or nothing, even if we only got something.
    // Even if not all the fields parsed and scanned correctly, the
    // returned data values should match up with each variables, where the
    // variable value is NaN if it did not scan.
    if (nparsed > 0)
	return sscanf->getNumberOfFields();
    return 0;
}




void
WxtSensor::
fromDOMElement(const xercesc::DOMElement* node)
    throw(InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);
}
