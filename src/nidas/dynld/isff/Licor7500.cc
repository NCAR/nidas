// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2014, Copyright University Corporation for Atmospheric Research
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

#include <nidas/dynld/isff/Licor7500.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/AsciiSscanf.h>

#include <sstream>
#include <limits>

using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,Licor7500)

Licor7500::Licor7500():
    _diagIndex(numeric_limits<unsigned int>::max()),
    _pcellIndex(numeric_limits<unsigned int>::max()),
    _tcellIndex(numeric_limits<unsigned int>::max()),
    _co2Index(numeric_limits<unsigned int>::max()),
    _h2oIndex(numeric_limits<unsigned int>::max()),
    _minDiag(-numeric_limits<float>::max()),
    _maxDiag(numeric_limits<float>::max()),
    _minPcell(-numeric_limits<float>::max()),
    _maxPcell(numeric_limits<float>::max()),
    _minTcell(-numeric_limits<float>::max()),
    _maxTcell(numeric_limits<float>::max())
{
}

Licor7500::~Licor7500()
{
}

void Licor7500::validate() throw(n_u::InvalidParameterException)
{
    list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
        SampleTag* tag = *ti;
        const vector<Variable*>& vars = tag->getVariables();
        vector<Variable*>::const_iterator vi = vars.begin();
        for (unsigned int i = 0; vi != vars.end(); ++vi,i++) {
            Variable* var = *vi;
            if (var->getName().find("diag") != std::string::npos) {
                _diagIndex = i;
                _minDiag = var->getMinValue();
                _maxDiag = var->getMaxValue();
                // reset the min,max for diag so that it isn't flagged.
                var->setMinValue(-numeric_limits<float>::max());
                var->setMaxValue(numeric_limits<float>::max());
            }
            else if (var->getName().find("Pcell") != std::string::npos) {
                _pcellIndex = i;
                _minPcell = var->getMinValue();
                _maxPcell = var->getMaxValue();
                // reset the min,max for Pcell so that it isn't flagged.
                var->setMinValue(-numeric_limits<float>::max());
                var->setMaxValue(numeric_limits<float>::max());
            }
            else if (var->getName().find("Tcell") != std::string::npos) {
                _tcellIndex = i;
                _minTcell = var->getMinValue();
                _maxTcell = var->getMaxValue();
                // reset the min,max for Tcell so that it isn't flagged.
                var->setMinValue(-numeric_limits<float>::max());
                var->setMaxValue(numeric_limits<float>::max());
            }
            else if (var->getName().find("raw") == std::string::npos) {
                if (var->getName().substr(0,3) == "co2") _co2Index = i;
                if (var->getName().substr(0,3) == "h2o") _h2oIndex = i;
            }
        }
    }
}

bool Licor7500::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    nidas::core::SerialSensor::process(samp,results);

    if (results.empty()) return false;

    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();

    float diag = floatNAN;
    if (slen > _diagIndex) diag = csamp->getDataValue(_diagIndex);

    float pcell = floatNAN;
    if (slen > _pcellIndex) pcell = csamp->getDataValue(_pcellIndex);

    float tcell = floatNAN;
    if (slen > _tcellIndex) tcell = csamp->getDataValue(_tcellIndex);

    // flag h2o,cor if Diag is outside the range _minDiag,_maxDiag
    if (isnan(diag) || diag < _minDiag || diag > _maxDiag ||
        isnan(pcell) || pcell < _minPcell || pcell > _maxPcell ||
        isnan(tcell) || tcell < _minTcell || tcell > _maxTcell) {

        SampleT<float>* news = getSample<float>(slen);
        news->setTimeTag(csamp->getTimeTag());
        news->setId(csamp->getId());

        float* nfptr = news->getDataPtr();
        const float* fptr = (const float*)csamp->getConstVoidDataPtr();

        memcpy(nfptr,fptr,slen*sizeof(float));

        if (_co2Index < slen) nfptr[_co2Index] = floatNAN;
        if (_h2oIndex < slen) nfptr[_h2oIndex] = floatNAN;

        csamp->freeReference();

        results.front() = news;
    }
    return true;
}

