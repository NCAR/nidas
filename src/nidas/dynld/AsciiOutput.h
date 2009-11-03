/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

    AsciiOutput();

    AsciiOutput(IOChannel* iochannel);

    virtual ~AsciiOutput() {}

    void requestConnection(SampleConnectionRequester* requester) throw();

    void connect(SampleSource* ) throw(nidas::util::IOException);
    /**
     * Set the format for character samples. Raw sensor samples
     * are character samples.
     */
    void setFormat(format_t val)
    {
        _format = val;
    }

    bool receive(const Sample* samp) throw();

protected:

    AsciiOutput* clone(IOChannel* iochannel);

    /**
     * Copy constructor, with a new IOChannel.
     */
    AsciiOutput(AsciiOutput&,IOChannel*);

    void printHeader() throw(nidas::util::IOException);

private:

    std::ostringstream _ostr;

    format_t _format;

    /**
     * Previous time tags by sample id. Used for displaying time diffs.
     */
    std::map<dsm_sample_id_t,dsm_time_t> _prevTT;

    bool _headerOut;

    /**
     * Copy constructor.
     */
    AsciiOutput(const AsciiOutput&);

};

}}	// namespace nidas namespace core

#endif
