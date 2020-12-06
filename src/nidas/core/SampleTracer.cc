// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2016 UCAR, NCAR, All Rights Reserved
 ********************************************************************
*/

#include "SampleTracer.h"

using namespace nidas::core;

SampleTracer::
SampleTracer(int level, const char* file, const char* function, int line):
    _context(level, file, function, line, "trace_samples"),
    _msg(&_context),
    _matcher()
{
    nidas::util::Logger* logger = nidas::util::Logger::getInstance();
    nidas::util::LogScheme scheme = logger->getScheme();
    std::string value = scheme.getParameter("trace_samples");
    _matcher.addCriteria(value);
}

