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
#include <nidas/util/UTime.h>
#include <nidas/util/Socket.h>

#include <string>
#include <list>
#include <memory>

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

    /**
     * Return true if this matcher can only match a single ID pair
     * (DSM,SID), meaning only one range has been added and it specifies
     * two IDs not equal to -1.
     **/
    bool
    exclusiveMatch();

    int 
    numRanges()
    {
        return _ranges.size();
    }

private:
    typedef std::map<dsm_sample_id_t, bool> id_lookup_t;
    typedef std::vector<RangeMatcher> range_matches_t;

    range_matches_t _ranges;
    id_lookup_t _lookup;
};


class NidasAppException : nidas::util::Exception
{
public:
    NidasAppException(const std::string& what) :
        Exception(what)
    {}
};


class NidasApp;
class NidasAppArg;

/**
 * Sets of arguments can be manipulated together by putting them into this
 * container type.  The container can be generated easily with operator|().
 **/
typedef std::vector<NidasAppArg*> nidas_app_arglist_t;


/**
 * A NidasAppArg identifies a particular kind of command-line argument
 * which can be handled by NidasApp.  The base class defines basic state
 * and behavior about the argument, then each type of argument has its own
 * instance member in the NidasApp.  Arguments can be subclassed from this
 * class to provide extra customization.  Only NidasApp can create
 * instances of NidasAppArg.
 **/
class NidasAppArg
{
public:
    bool enabled;

    /**
     * Provide an alternative flag for the argument.
     **/
    void
    setDeprecatedFlag(const std::string& flag)
    {
        deprecatedFlag = flag;
    }

    /**
     * Provide conversion to an arglist so a single NidasAppArg can be
     * passed where an arglist is expected.
     **/
    operator nidas_app_arglist_t()
    {
        nidas_app_arglist_t args;
        args.push_back(this);
        return args;
    }

protected:
    NidasAppArg(const std::string& flag_ = "") :
        enabled(false),
        flag(flag_),
        deprecatedFlag()
    {}

    virtual
    ~NidasAppArg()
    {}

    bool
    accept(const std::string& flag)
    {
        return enabled && 
            !flag.empty() &&
            (flag == this->flag || flag == this->deprecatedFlag);
    }

private:
    NidasAppArg&
    operator=(const NidasAppArg&);
    NidasAppArg(const NidasAppArg&);

    std::string flag;
    std::string deprecatedFlag;

    friend NidasApp;
};


class NidasAppInputFilesArg : public NidasAppArg
{
public:
    bool allowFiles;
    bool allowSockets;

    void
    setDefaultInput(const std::string& spec, int default_port_ = 0)
    {
        default_input = spec;
        if (default_port_)
            default_port = default_port_;
    }

private:

    NidasAppInputFilesArg() :
        NidasAppArg(),
        allowFiles(true),
        allowSockets(true),
        default_input(),
        default_port(0)
    {
    }

    std::string default_input;
    int default_port;

    virtual
    ~NidasAppInputFilesArg()
    {}

    friend NidasApp;
};


nidas_app_arglist_t
operator|(nidas_app_arglist_t arglist, NidasAppArg& arg2)
{
    arglist.push_back(&arg2);
    return arglist;
}


nidas_app_arglist_t
operator|(NidasAppArg& arg1, NidasAppArg& arg2)
{
    nidas_app_arglist_t result;
    return result | arg1 | arg2;
}


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

    NidasAppArg XmlHeaderFile;
    NidasAppArg LogLevel;
    NidasAppArg Help;
    NidasAppArg ProcessData;
    NidasAppArg StartTime;
    NidasAppArg EndTime;
    NidasAppArg SampleRanges;
    NidasAppArg Version;
    NidasAppInputFilesArg InputFiles;
    NidasAppArg OutputFiles;

    NidasApp(const std::string& name);

    std::string
    getName()
    {
        return _appname;
    }

    void
    enableArguments(const nidas_app_arglist_t& arglist)
    {
        nidas_app_arglist_t::const_iterator it;
        for (it = arglist.begin(); it != arglist.end(); ++it)
        {
            (*it)->enabled = true;
        }
    }

    /**
     * Set the options from the given argument list.  Raise
     * NidasAppException if there was an error parsing any of the options.
     * Recognized arguments are removed from the argument list, so upon
     * return it only contains the arguments not handled by NidasApp.
     * Position arguments like input sockets and file names are not handled
     * here, since they cannot be differentiated from app-specific
     * arguments yet.  Instead those arguments can be passed explicitly to
     * the parseInputs() method.
     **/
    void
    parseArguments(std::vector<std::string>& args) throw (NidasAppException);

    void
    parseLogLevel(const std::string& optarg) throw (NidasAppException);

    nidas::util::UTime
    parseTime(const std::string& optarg);

    void
    parseInputs(std::vector<std::string>& inputs,
                const std::string& default_input = "",
                int default_port = 0) throw (NidasAppException);

    /**
     * Parse an output specifier in the form
     * <strptime-filename-pattern>[@<length>[smh]].
     **/
    void
    parseOutput(const std::string& optarg) throw (NidasAppException);

    std::string
    outputFileName()
    {
        return _outputFileName;
    }

    /**
     * Return the output file length in seconds.
     **/
    int
    outputFileLength()
    {
        return _outputFileLength;
    }

    int
    logLevel()
    {
        return _logLevel;
    }

    bool
    processData()
    {
        return _processData;
    }

    std::string
    xmlHeaderFile()
    {
        return _xmlFileName;
    }

    bool
    interrupted();

#ifdef notdef
    setupOptions()
    setNumInputs(n)
    setNumOutputs(n)
    usage()
    main()?
    run()?
#endif

    static void
    setupSignals();

private:

    std::string _appname;

    int _logLevel;

    bool _processData;

    std::string _xmlFileName;

    enum idfmt _idFormat;

    SampleMatcher _sampleMatcher;

    nidas::util::UTime _startTime;
 
    nidas::util::UTime _endTime;

    std::list<std::string> _dataFileNames;

    std::auto_ptr<nidas::util::SocketAddress> _sockAddr;

    std::string _outputFileName;
    int _outputFileLength;
};



}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_NIDASAPP_H
