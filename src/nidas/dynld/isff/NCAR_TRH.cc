// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/NCAR_TRH.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/AsciiSscanf.h>

#include <sstream>
#include <limits>

using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NCAR_TRH)

NCAR_TRH::NCAR_TRH():
    _ifanIndex(99),
    _minIfan(-numeric_limits<float>::max()),
    _maxIfan(numeric_limits<float>::max())
{
}

NCAR_TRH::~NCAR_TRH()
{
}

void NCAR_TRH::validate() throw(n_u::InvalidParameterException)
{
    list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
        SampleTag* tag = *ti;
        const vector<Variable*>& vars = tag->getVariables();
        vector<Variable*>::const_iterator vi = vars.begin();
        for (unsigned int i = 0; vi != vars.end(); ++vi,i++) {
            Variable* var = *vi;
            if (var->getName().substr(0,4) == "Ifan") {
                _ifanIndex = i;
                _minIfan = var->getMinValue();
                _maxIfan = var->getMaxValue();
                var->setMinValue(-numeric_limits<float>::max());
                var->setMaxValue(numeric_limits<float>::max());
            }
        }
    }
}

bool NCAR_TRH::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    nidas::core::SerialSensor::process(samp,results);

    if (results.empty()) return false;

    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();

    if (slen > _ifanIndex) {
        float ifan = csamp->getDataValue(_ifanIndex);

        // flag T,RH if Ifan is less than _minIfan
        if (ifan < _minIfan || ifan > _maxIfan) {

            SampleT<float>* news = getSample<float>(slen);
            news->setTimeTag(csamp->getTimeTag());
            news->setId(csamp->getId());

            float* nfptr = news->getDataPtr();

            unsigned int i;
            // flag any values other then Ifan
            for (i = 0; i < slen; i++) {
                if (i != _ifanIndex) nfptr[i] = floatNAN;
                else nfptr[i] = csamp->getDataValue(i);
            }

            csamp->freeReference();

            results.front() = news;
        }
    }
    return true;
}

