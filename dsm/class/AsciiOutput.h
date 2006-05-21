/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-21 08:31:42 -0700 (Tue, 21 Feb 2006) $

    $LastChangedRevision: 3297 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/src/data_dump.cc $
 ********************************************************************

*/

#include <SampleOutput.h>

#include <iostream>

#ifndef DSM_ASCIIOUTPUT_H
#define DSM_ASCIIOUTPUT_H

namespace dsm {

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
	throw(atdUtil::IOException);

    void connect() throw(atdUtil::IOException);
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

    void printHeader() throw(atdUtil::IOException);

private:

    std::ostringstream ostr;

    format_t format;

    /**
     * Previous time tags by sample id. Used for displaying time diffs.
     */
    std::map<dsm_sample_id_t,dsm_time_t> prevTT;

    bool headerOut;

};

}

#endif
