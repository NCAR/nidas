// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2016 UCAR, NCAR, All Rights Reserved
 ********************************************************************
*/
#ifndef NIDAS_CORE_SAMPLETRACER_H
#define NIDAS_CORE_SAMPLETRACER_H

#include "SampleMatcher.h"

namespace nidas { namespace core {


/**
 * SampleTracer uses a Logger to log messages about samples as they are
 * encountered in the code.  The samples are selected by ID using a
 * SampleMatcher, whose criteria are specified in the 'trace_samples' log
 * parameter.
 **/
class SampleTracer
{
public:
    SampleTracer(int level, const char* file, const char* function, int line);

    inline bool
    active(const Sample* samp)
    {
        return _context.active() && _matcher.match(samp);
    }

    inline bool
    active(dsm_sample_id_t id)
    {
        return _context.active() && _matcher.match(id);
    }

    inline static std::string
    format_time(dsm_time_t tt)
    {
        return nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%4f");
    }

    inline static std::string
    format_time(dsm_time_t tt, const std::string& format)
    {
        return nidas::util::UTime(tt).format(true, format);
    }

    /**
     * Format a basic log message tracing the given the sample, using text
     * as a prefix, then return a reference to this instance's LogMessage
     * so more information can be streamed to it.  The 'endlog' manipulator
     * must be streamed to the return value to actually log the message.
     * For example:
     *
     * @code
     * static SampleTracer st;
     * ...
     * 
     * if (st.active(samp))
     *    st.msg(samp, "received") << endlog;
     * @endcode
     *
     * With no arguments, no message is added, just the LogMessage
     * reference is returned, allowing more information to be streamed to
     * an existing message.
     **/
    inline nidas::util::LogMessage&
    msg(const Sample* samp = 0, const std::string& text = "")
    {
        if (samp)
        {
            dsm_time_t tt = samp->getTimeTag();
            dsm_sample_id_t sid = samp->getId();
            _msg << text
                 << "[" << GET_DSM_ID(sid) << ',' << GET_SPS_ID(sid) << "]"
                 << "@" << format_time(tt);
        }
        return _msg;
    }

    inline nidas::util::LogMessage&
    msg(dsm_time_t tt, dsm_sample_id_t sid, const std::string& text = "")
    {
        _msg << text
             << "[" << GET_DSM_ID(sid) << ',' << GET_SPS_ID(sid) << "]"
             << "@" << format_time(tt);
        return _msg;
    }

private:
    nidas::util::LogContext _context;
    nidas::util::LogMessage _msg;
    nidas::core::SampleMatcher _matcher;

};

}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_SAMPLETRACER_H
