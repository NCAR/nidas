// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2014 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2014-08-11 14:42:08 -0600 (Mon, 11 Aug 2014) $

    $LastChangedRevision: 7091 $

    $LastChangedBy: granger $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/DSMSensor.h $
 ********************************************************************

*/
#ifndef NIDAS_CORE_NIDASAPP_H
#define NIDAS_CORE_NIDASAPP_H

#include <nidas/core/SampleTag.h>

#include <string>
#include <list>

namespace nidas { namespace core {

/**
 * Provide criteria for matching samples by DSM and sample id, and perhaps
 * someday by time and type.
 **/
class SampleMatcher
{
    struct RangeMatcher
    {
        // By default a RangeMatcher matches all IDs.
        RangeMatcher();

        RangeMatcher(int d1, int d2, int s1, int s2, int inc);

        int dsm1;
        int dsm2;
        int sid1;
        int sid2;
        bool include;
    };

public:
    SampleMatcher();

    bool
    addCriteria(const std::string& ctext);

    bool
    match(dsm_sample_id_t id);

private:
    typedef std::map<dsm_sample_id_t, bool> id_lookup_t;
    typedef std::vector<RangeMatcher> range_matches_t;

    range_matches_t _ranges;
    id_lookup_t _lookup;
};




/**
 * A class to handle common options for NIDAS applications.  The
 * application specifies which of the options are valid and it can specify
 * defaults if necessary, and then command-line arguments can be parsed and
 * their settings retrieved through this instance.  This class allows for
 * consistent option letters and syntax across NIDAS applications.
 * Applications can extend this class with their own options, either as a
 * subclass or by delegation to an instance.
 **/
class NidasApp
{
public:

    typedef enum idfmt {DECIMAL, HEX_ID, OCTAL } id_format_t;

    enum ArgumentMask {
        NoArgument = 0,
        XmlHeaderArgument = 1,
        LogLevelArgument = 2,
        HelpArgument = 4,
        InputArgument = 8,
        ProcessArgument = 16,
        StartTimeArgument = 32,
        EndTimeArgument = 64,
        SampleIDArgument = 128
    };

    NidasApp();

    void
    setArguments(unsigned int mask);

    void
    parseArguments(const char** argv, int argc);

    int
    logLevel()
    {
        return _logLevel;
    }

    std::string
    xmlHeaderFile()
    {
        return _xmlFileName;
    }

#ifdef notdef
    setupOptions()
    setNumInputs(n)
    setNumOutputs(n)
    usage()
    main()?
    run()?
#endif

private:

    unsigned int _allowedArguments;

    int _logLevel;

    bool _processData;

    std::string _xmlFileName;

    enum idfmt _idFormat;

    SampleMatcher _sampleMatcher;

};



}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_NIDASAPP_H
