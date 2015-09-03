/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/* -*- mode: c++; c-basic-offset: 4; -*- */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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
/********************************************************************

*/

#ifndef NIDAS_DYNLD_PSQL_PSQLSAMPLEOUTPUT_H
#define NIDAS_DYNLD_PSQL_PSQLSAMPLEOUTPUT_H

#include <nidas/dynld/SampleOutputStream.h>
#include "PSQLChannel.h"

#include <map>

namespace nidas { namespace dynld { namespace psql {

class PSQLSampleOutput : public nidas::dynld::SampleOutputStream
{
public:

    PSQLSampleOutput();

    PSQLSampleOutput(const PSQLSampleOutput&);

    virtual ~PSQLSampleOutput();

    PSQLSampleOutput* clone(IOChannel* iochannel = 0) const;

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    bool isRaw() const { return false; }

    void setPseudoPort(int val) {}

    int getPseudoPort() const { return 0; }

    void setDSMConfigs(const std::list<const DSMConfig*>& val);

    void addDSMConfig(const DSMConfig*);

    const std::list<const DSMConfig*>& getDSMConfigs() const;

    void addSampleTag(const SampleTag* tag)
    	throw(nidas::util::InvalidParameterException);

    void requestConnection(SampleConnectionRequester*)
        throw(nidas::util::IOException);

    void connected(SampleOutput* origout, SampleOutput* newout) throw();

    void connect() throw(nidas::util::IOException);

    void connected(IOChannel* output) throw();

    void init() throw();

    bool receive(const Sample*) throw();

    int getFd() const { return -1; }

    void flush() throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    void submitCommand(const std::string& command) throw(nidas::util::IOException);

    void createTables() throw(nidas::util::IOException);

    void dropAllTables() throw();

    void initializeGlobalAttributes() throw(nidas::util::IOException);

    void addVariable(const Variable* var) throw(nidas::util::IOException);

    void addCategory(const std::string& varName, const std::string& category)
    	throw(nidas::util::IOException);

    std::string name;

    SampleConnectionRequester* connectionRequester;

    std::list<const DSMConfig*> dsms;

    PSQLChannel* psqlChannel;

    // std::list<const SampleTag*> sampleTags;

    std::map<float,const SampleTag*> tagsByRate;

    std::map<float,std::string> tablesByRate;

    std::map<dsm_sample_id_t,const SampleTag*> tagsById;

    float missingValue;

    bool first;

    int dberrors;
};

}}}

#endif // NIDAS_DYNLD_PSQL_PSQLSAMPLEOUTPUT_H

