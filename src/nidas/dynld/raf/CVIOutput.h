/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/CVIOutput.h $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAF_CVIOUTPUT_H
#define NIDAS_DYNLD_RAF_CVIOUTPUT_H

#include <nidas/core/SampleOutput.h>
#include <nidas/core/DerivedDataClient.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

class CVIOutput: public SampleOutputBase, public DerivedDataClient
{
public:

    CVIOutput(IOChannel* iochannel=0);

    /**
     * Copy constructor.
     */
    CVIOutput(const CVIOutput&);

    /**
     * Copy constructor, with a new IOChannel.
     */
    CVIOutput(const CVIOutput&,IOChannel*);

    ~CVIOutput();

    CVIOutput* clone(IOChannel* iochannel=0) const;

    void addSampleTag(const SampleTag*);

    void requestConnection(SampleConnectionRequester* requester)
	throw(nidas::util::IOException);

    void connect() throw(nidas::util::IOException);

    bool receive(const Sample* samp) throw();

    void derivedDataNotify(const DerivedDataReader * s) throw();

private:

    std::ostringstream ostr;

    std::vector<const Variable*> _variables;

    dsm_time_t _tt0;

    /**
     * True airspeed from DerivedDataReader.
     */
    float _tas;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
