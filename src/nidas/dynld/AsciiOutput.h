/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-21 08:31:42 -0700 (Tue, 21 Feb 2006) $

    $LastChangedRevision: 3297 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/src/data_dump.cc $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ASCIIOUTPUT_H
#define NIDAS_DYNLD_ASCIIOUTPUT_H

#include <nidas/core/SampleOutput.h>

#include <iostream>

namespace nidas { namespace dynld {

using namespace nidas::core;

class AsciiOutput: public SampleOutputBase
{
public:

    typedef enum format { DEFAULT, ASCII, HEX, SIGNED_SHORT, UNSIGNED_SHORT,
    	FLOAT, IRIG } format_t;

    AsciiOutput(IOChannel* iochannel=0);

    /**
     * Copy constructor.
     */
    AsciiOutput(const AsciiOutput&);

    /**
     * Copy constructor, with a new IOChannel.
     */
    AsciiOutput(const AsciiOutput&,IOChannel*);

    virtual ~AsciiOutput() {}

    AsciiOutput* clone(IOChannel* iochannel=0) const;

    void requestConnection(SampleConnectionRequester* requester)
	throw(nidas::util::IOException);

    void connect() throw(nidas::util::IOException);
    /**
     * Set the format for character samples. Raw sensor samples
     * are character samples.
     */
    void setFormat(format_t val)
    {
        format = val;
    }

    bool receive(const Sample* samp) throw();

protected:

    void printHeader() throw(nidas::util::IOException);

private:

    std::ostringstream ostr;

    format_t format;

    /**
     * Previous time tags by sample id. Used for displaying time diffs.
     */
    std::map<dsm_sample_id_t,dsm_time_t> prevTT;

    bool headerOut;

};

}}	// namespace nidas namespace core

#endif
