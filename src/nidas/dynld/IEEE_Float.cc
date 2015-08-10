// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include "IEEE_Float.h"

#include <nidas/core/Parameter.h>
#include <nidas/core/Variable.h>

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(IEEE_Float)

IEEE_Float::IEEE_Float(): DSMSerialSensor(),
    _endian(nidas::util::EndianConverter::EC_LITTLE_ENDIAN),
    _converter(0),_sampleTag(0),_nvars(0)
{
}

void IEEE_Float::init() throw(n_u::InvalidParameterException)
{
    _converter = n_u::EndianConverter::getConverter(_endian,
            n_u::EndianConverter::getHostEndianness());
}

void IEEE_Float::validate() throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::validate();

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "endian") {
            bool pok = parameter->getType() == Parameter::STRING_PARAM &&
                parameter->getLength() == 1;
            if (pok) {
                if (parameter->getStringValue(0) == "little") {
                    _endian = nidas::util::EndianConverter::EC_LITTLE_ENDIAN;
                    pok = true;
                }
                else if (parameter->getStringValue(0) == "big") {
                    _endian = nidas::util::EndianConverter::EC_BIG_ENDIAN;
                    pok = true;
                }
            }
            if (!pok) throw n_u::InvalidParameterException(getName(),
                "endian parameter",
                "must be one string: \"little\" (default), or \"big\"");
        }
        else throw n_u::InvalidParameterException(getName(),
                "unknown parameter",parameter->getName());
    }

    list<SampleTag*>& tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() + " can only create one sample");

    std::list<SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        _sampleTag = *si;
        _nvars = _sampleTag->getVariables().size();
    }
}

bool IEEE_Float::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
    assert(samp->getType() == CHAR_ST);

    const char* dp = (const char*) samp->getConstVoidDataPtr();
    const char* deod = dp + samp->getDataLength();

    SampleT<float>* outs = getSample<float>(_nvars);
    outs->setTimeTag(samp->getTimeTag() - getLagUsecs());
    outs->setId(_sampleTag->getId());
    float* dout = outs->getDataPtr();
    const vector<Variable*>& vars = _sampleTag->getVariables();

    int iv = 0;
    for ( ; iv < _nvars && dp + sizeof(float) <= deod; iv++) {
        Variable* var = vars[iv];
        float val = _converter->floatValue(dp);
        if (val == var->getMissingValue()) val = floatNAN;
        else {
            if (getApplyVariableConversions()) {
                VariableConverter* conv = var->getConverter();
                if (conv) val = conv->convert(samp->getTimeTag(),val);
            }

            /* Screen values outside of min,max after the conversion */
            if (val < var->getMinValue() || val > var->getMaxValue()) 
                val = floatNAN;
        }
        *dout++ = val;
        dp += sizeof(float);
    }
    for ( ; iv < _nvars; iv++) *dout++ = floatNAN;

    results.push_back(outs);
    return true;
}
