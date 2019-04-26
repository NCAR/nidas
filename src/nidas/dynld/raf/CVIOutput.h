// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_CVIOUTPUT_H
#define NIDAS_DYNLD_RAF_CVIOUTPUT_H

#include <nidas/core/SampleOutput.h>
#include <nidas/core/DerivedDataClient.h>

namespace nidas {

namespace core {
class Variable;
}

namespace dynld { namespace raf {

using namespace nidas::core;

class CVIOutput: public SampleOutputBase, public DerivedDataClient
{
public:

    CVIOutput();

    CVIOutput(IOChannel* iochannel,SampleConnectionRequester* rqstr=0);

    ~CVIOutput();

    /**
     * Implementation of SampleClient::flush().
     */
    void flush() throw() {}

    CVIOutput* clone(IOChannel* iochannel=0);

    void addRequestedSampleTag(SampleTag*)
        throw(nidas::util::InvalidParameterException);

    void requestConnection(SampleConnectionRequester* requester) throw();

    bool receive(const Sample* samp) throw();

    void derivedDataNotify(const DerivedDataReader * s) throw();

    void setIOChannel(IOChannel* val);

protected:
    /**
     * Copy constructor, with a new IOChannel.
     */
    CVIOutput(CVIOutput&,IOChannel*);

private:

    std::ostringstream _ostr;

    std::vector<const Variable*> _variables;

    dsm_time_t _tt0;

    /**
     * True airspeed from DerivedDataReader.
     */
    float _tas;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
