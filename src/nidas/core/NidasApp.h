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

#include "SampleTag.h"
#include <nidas/util/UTime.h>
#include <nidas/util/Socket.h>

#include <string>
#include <list>
#include <memory>

namespace nidas { namespace core {

class Project;

/**
 * Match samples according to DSM and Sample ID ranges, and configure the
 * ranges with criteria in text format.
 **/
class SampleMatcher
{
    struct RangeMatcher
    {
        /**
         * Construct a RangeMatcher with the given range endpoints: d1 <=
         * DSM ID <= d2, s1 <= Sample ID <= s2, and if @p inc is false,
         * then matching sample IDs are excluded rather than included.
         **/ 
        RangeMatcher(int d1, int d2, int s1, int s2, int inc);

        int dsm1;
        int dsm2;
        int sid1;
        int sid2;
        bool include;
    };

public:

    /**
     * Construct an empty SampleMatcher with no ranges.  An empty
     * SampleMatcher implicitly matches all samples.
     **/
    SampleMatcher();

    /**
     * Add a sample range using this syntax:
     * @verbatim
     *   [^]{<d1>[-<d2>|*},{<s1>[-<s2>]|*}
     * @endverbatim
     * The leading '^' will exclude any sample IDs in the given range
     * instead of including them.  The '*' matches any ID.  The older
     * convention where -1 matches all IDs is deprecated but still
     * supported.
     **/
    bool
    addCriteria(const std::string& ctext);

    /**
     * Return true if the given @p id satisfies the current range criteria.
     * Search the ranges for one which includes this id, then return true
     * if the range is an inclusion and otherwise false.  The outcome is
     * cached for future lookups, but the cache is cleared if the criteria
     * change.
     **/
    bool
    match(dsm_sample_id_t id);

    /**
     * Return true if this sample matches all the criteria in this matcher,
     * both sample ids and time range.
     **/
    bool
    match(const Sample* samp);

    /**
     * Return true if this matcher can only match a single ID pair
     * (DSM,SID), meaning only one range has been added and it specifies
     * two specific positive IDs.
     **/
    bool
    exclusiveMatch();

    /**
     * The number of ranges added to this SampleMatcher.
     **/
    int 
    numRanges()
    {
        return _ranges.size();
    }

    /**
     * Set the time before which samples will not match.
     **/
    void
    setStartTime(nidas::util::UTime start)
    {
        _startTime = start;
    }

    nidas::util::UTime
    getStartTime()
    {
        return _startTime;
    }

    /**
     * Set the time after which samples will not match.
     **/
    void
    setEndTime(nidas::util::UTime end)
    {
        _endTime = end;
    }

    nidas::util::UTime
    getEndTime()
    {
        return _endTime;
    }

private:
    typedef std::map<dsm_sample_id_t, bool> id_lookup_t;
    typedef std::vector<RangeMatcher> range_matches_t;

    range_matches_t _ranges;
    id_lookup_t _lookup;
    nidas::util::UTime _startTime;
    nidas::util::UTime _endTime;
};


/**
 * The NidasApp class throws a NidasAppException when command-line options
 * do not parse.
 **/
class NidasAppException : public nidas::util::Exception
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
 * container type.  The container can be generated using operator|().
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
    /**
     * Indicates whether this argument is accepted by it's NidasApp
     * instance.
     **/
    bool enabled;

    /**
     * Provide an alternative but deprecated flag for the argument.
     **/
    void
    setDeprecatedFlag(const std::string& flag)
    {
        deprecatedFlag = flag;
    }

    /**
     * Set whether short flags are enabled or not.  Pass @p enable as false
     * to disable short flags and require only long flags instead.  By
     * default short flags are enabled.
     **/
    void
    acceptShortFlag(bool enable)
    {
        enableShortFlag = enable;
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

    /**
     * Return the usage string for this particular argument.
     **/
    std::string
    usage()
    {
        return usageString;
    }

protected:
    NidasAppArg(const std::string& flag_ = "", 
                const std::string& longflag_ = "",
                const std::string& usage = "",
                const std::string& spec = "") :
        enabled(false),
        flag(flag_),
        longFlag(longflag_),
        usageString(usage),
        specifier(spec),
        deprecatedFlag(),
        enableShortFlag(true)
    {}

    virtual
    ~NidasAppArg()
    {}

    bool
    accept(const std::string& flag)
    {
        return enabled && 
            !flag.empty() &&
            ((enableShortFlag && (flag == this->flag)) ||
             (flag == longFlag) ||
             (flag == this->deprecatedFlag));
    }

    void
    setUsageString(const std::string& text)
    {
        usageString = text;
    }

private:

    // Prevent the public NidasAppArg members of NidasApp from being
    // replaced with other arguments.
    NidasAppArg&
    operator=(const NidasAppArg&);
    NidasAppArg(const NidasAppArg&);

    std::string flag;
    std::string longFlag;
    std::string usageString;
    std::string specifier;
    std::string deprecatedFlag;
    bool enableShortFlag;

    friend class NidasApp;
};


/**
 * Extend NidasAppArg so the default input specifier and port can be
 * customized, which in turn updates the usage string.
 **/
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
        updateUsage();
    }

private:

    NidasAppInputFilesArg() :
        NidasAppArg("", "", "", "input-url [...]"),
        allowFiles(true),
        allowSockets(true),
        default_input(),
        default_port(0)
    {
    }

    void
    updateUsage();

    std::string default_input;
    int default_port;

    virtual
    ~NidasAppInputFilesArg()
    {}

    friend class NidasApp;
};


inline nidas_app_arglist_t
operator|(nidas_app_arglist_t arglist, NidasAppArg& arg2)
{
    arglist.push_back(&arg2);
    return arglist;
}


inline nidas_app_arglist_t
operator|(NidasAppArg& arg1, NidasAppArg& arg2)
{
    nidas_app_arglist_t result;
    return result | arg1 | arg2;
}


/**
 * Handle common options for NIDAS applications.  The application specifies
 * which of the options are valid and it can specify defaults if necessary,
 * and then command-line arguments can be parsed and their settings
 * retrieved through this instance.  This class allows for consistent
 * option letters and syntax across NIDAS applications.  Applications can
 * extend this class with their own options, either as a subclass or by
 * delegation to an instance.
 *
 * There are several NIDAS applications with common options that can be
 * consolidated in this class:
 * 
 * - data_dump
 * - data_stats
 * - nidsmerge
 * - sensor_extract
 * - statsproc (uses -B and -E for time range)
 * - data_nc
 * - dsm (esp logging)
 * - dsm_server (esp logging)
 * - sync_server
 * - nimbus
 *
 * The goal is to keep syntax for common arguments consistent across
 * applications.
 *
 * <table>
<tr>
<td><b>Standard option</b></td>
<td><b>Description</b></td>
<td><b>Replacement Syntax</b></td>
</tr>
<tr>
<td>-i dsmid,sampleid</td><td>Sample IDs to include.</td><td></td>
</tr>
<tr>
<td>-x dsmid,sampleid</td><td>Sample IDs to exclude.</td><td>-i ^dsmid,sampid</td>
</tr>
<tr>
<td>-l loglevel</td><td>Numeric log level.</td><td>-l number_or_name</td>
</tr>
<tr>
<td>-l interval</td><td>Output file interval.</td><td>output\@interval</td>
</tr>
<tr>
<td>-h</td><td>Help</td><td></td>
</tr>
<tr>
<td>-p</td><td>Process samples</td><td></td>
</tr>
<tr>
<td>-X</td><td>Print Hex IDs</td><td></td>
</tr>
<tr>
<td>-v</td><td>Report version.</td><td></td>
</tr>
<tr>
<td>-x xmlfile</td><td>Explicit header file.</td><td></td>
</tr>
<tr>
<td>input</td><td>Socket or files.</td><td></td>
</tr>
<tr>
<td>output[\@interval]</td><td>Output pattern, interval</td><td></td>
</tr>
<tr>
<td>-s starttime</td><td>Skip samps before <em>start</em></td><td></td>
</tr>
<tr>
<td>-e endtime</td><td>Skip samps after <em>end</em></td><td></td>
</tr>
</table>
 *
 * Notes:
 *
 * ### -x exclude samples ###
 *
 * Where -x is accepted but deprecated, -x A,B translates to -i ^A,B.
 *
 * ### -l interval ###
 *
 * Since -l conflicts with log level and output file length, the separate
 * -l for file length is being deprecated in favor of specifying the length
 * in the output file pattern:
 *
 * _strptime_pattern_[\@_number_[_units_]]
 *
 * Units can be (s)econds, (m)inutes, (h)ours, seconds is the default.
 * Maybe later add units for size like kb, mb.
 *
 * The -l option needs to be accepted for a while, perhaps differntiated
 * automatically by whether it's a number greater than 10 (for apps which
 * enable that option).  But it should produce a warning.  Is there ever a
 * case where the output-file-length needs to be specified without an
 * output file name pattern?  Is there a reasonable default filename
 * pattern if the output is simply @<interval>?
 *
 * ### -l loglevel ###
 *
 * _loglevel_ can be a number or the name of a log level:
 *
 * 7=debug,6=info,5=notice,4=warn,3=err
 *
 * The default is *info*.  Eventually the log argument could also be the
 * name of a logging scheme in the XML file.
 * 
 * ### -s translating sample ids ###
 *
 * `sensor_extract` uses -s to map id selection to a new id, eg 10,1,10,3.
 * Rather than add another option letter, maybe it would be better to
 * extend the -i syntax: -i 10,1=10,3 where the = part is only allowed if
 * specifically enabled.  Then sensor_extract just needs a way to enable
 * that extension and to get to that mapping.  Or, maybe the mapping can be
 * completely internal to the sample filter: if app passes the sample
 * pointer to the filter, the filter can modify the sample id as specified
 * after checking that the sample is selected.
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

    /**
     * Give the NidasApp instance a name, to be used for the usage info and
     * the logging scheme.
     **/
    NidasApp(const std::string& name);

    /**
     * If this instance is the current application instance, then the
     * application instance is reset to null when this is destroyed.
     **/
    ~NidasApp();

    std::string
    getName()
    {
        return _appname;
    }

    /**
     * An instance of NidasApp can be set as an application-wide instance,
     * like an application context, which other parts of the application
     * can retrieve with getApplicationInstance().
     **/
    void
    setApplicationInstance();

    /**
     * Return the current application instance.  See
     * setApplicationInstance().
     **/
    static NidasApp*
    getApplicationInstance();

    /**
     * Set the enabled flag on the given list of NidasApp instances.  There
     * is no check that the passed arguments are actually members of this
     * NidasApp instance, but that should usually be the case.  The arglist
     * can be created implicitly from the NidasAppArg instances, as in this
     * example:
     *
     * @code
     * NidasApp app('na');
     * app.enableArguments(app.LogLevel | app.ProcessData);
     * @endcode
     **/
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
     * Call acceptShortFlag(false) on the given list of NidasApp instances,
     * meaning only the long flag will be recognized.  There is no check
     * that the passed arguments are actually members of this NidasApp
     * instance, but that should usually be the case.
     **/
    void
    requireLongFlag(const nidas_app_arglist_t& arglist, bool require=true)
    {
        nidas_app_arglist_t::const_iterator it;
        for (it = arglist.begin(); it != arglist.end(); ++it)
        {
            (*it)->acceptShortFlag(!require);
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

    /**
     * Parse a number 1-7 or a string into a LogLevel and set that level
     * for this NidasApp instance.
     **/
    void
    parseLogLevel(const std::string& optarg) throw (NidasAppException);

    nidas::util::UTime
    parseTime(const std::string& optarg);

    /**
     * Parse one or more input URLs from a list of non-option command-line
     * arguments.  The @p default_input and @p default_port can be passed
     * here to override the defaults set in the InputFiles argument
     * instance.  If no inputs are provided, then the defaults are used.
     * If no default is specified, then inputsProvided() will return false.
     * An exception is not thrown just because no inputs were provided.
     **/
    void
    parseInputs(std::vector<std::string>& inputs,
                std::string default_input = "",
                int default_port = 0) throw (NidasAppException);

    /**
     **/
    bool
    inputsProvided()
    {
        return _dataFileNames.size() > 0  || _sockAddr.get();
    }

    /**
     * Parse an output specifier in the form
     * <strptime-filename-pattern>[@<length>[smh]].
     **/
    void
    parseOutput(const std::string& optarg) throw (NidasAppException);

    /**
     * Return just the output file pattern specified by an output option,
     * without the file length specifier.
     **/
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

    nidas::util::UTime
    getStartTime()
    {
        return _startTime;
    }
 
    nidas::util::UTime
    getEndTime()
    {
        return _endTime;
    }

    bool
    interrupted();

    /**
     * Return a pointer to the application-wide Project instance.  The lifetime
     * is tied to this NidasApp instance.
     **/
    Project*
    getProject();

    /**
     * Return a usage string describing the arguments accepted by this
     * application.
     **/
    std::string
    usage();

    /**
     * Setup signal handling for HUP, INT, and TERM signals.  If non-zero,
     * @p callback is a function which will be called when the application
     * receives an interrupt signal.  The callback is called
     * asynchronously, directly from the signal handler function.
     **/
    static void
    setupSignals(void (*callback)() = 0);

    /**
     * Return true when a help option was parsed, meaning the usage info
     * has been requested.
     **/
    bool
    helpRequested()
    {
        return _help;
    }

    /**
     * Store the format in which sample IDs should be shown, as listed in
     * the id_format_t enum.
     **/
    void
    setIdFormat(id_format_t idt);

    id_format_t
    getIdFormat()
    {
        return _idFormat;
    }

    static
    std::ostream&
    formatSampleId(std::ostream&, id_format_t, dsm_sample_id_t);

    /**
     * Use this method to access the SampleMatcher instance for this
     * NidasApp.  After the SampleMatcher is configured by parsing
     * command-line options, it can be used to select sample IDs like so:
     *
     * @code
     * bool DumpClient::receive(const Sample* samp) throw()
     * {
     *   dsm_sample_id_t sampid = samp->getId();
     *   if (!app.sampleMatcher().match(sampid))
     *   {
     *     return false;
     *   }
     *   ...
     * }
     * @endcode
     **/
    SampleMatcher&
    sampleMatcher()
    {
        return _sampleMatcher;
    }

    /**
     * Return the list of filenames parsed from the input URLs with
     * parseInputs().  If the input URL specified a network socket, then
     * this list is empty.  See socketAddress().
     **/
    std::list<std::string>&
    dataFileNames()
    {
        return _dataFileNames;
    }

    /**
     * If parseInputs() parsed a network socket specifier, then this method
     * returns a pointer to the corresponding SocketAddress.  Otherwise it
     * returns null.  The NidasApp instance owns the pointer.
     **/    
    nidas::util::SocketAddress*
    socketAddress()
    {
        return _sockAddr.get();
    }

private:

    static NidasApp* application_instance;

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

    bool _help;

    bool _deleteProject;
};


/**
 * Convert vector<string> args to dynamically allocated (argc, argv) pair
 * which will be freed when the instance is destroyed.  This is useful for
 * passing leftover NidasApp command-line arguments to getopt() functions:
 *
 * @code
 * NidasApp app('data_dump');
 * vector<string> args(argv, argv+argc);
 * // Parse standard arguments and leave the rest.
 * app.parseArguments(args);
 * NidasAppArgv left(args);
 * int opt_char;
 * while ((opt_char = getopt(left.argc, left.argv, "...")) != -1) {
 *    ...
 * }
 * @endcode
 **/
struct NidasAppArgv
{
    NidasAppArgv(const std::vector<std::string>& args) :
        vargv(), argv(0), argc(0)
    {
        argc = args.size();
        for (int i = 0; i < argc; ++i)
        {
            char* argstring = strdup(args[i].c_str());
            vargv.push_back(argstring);
        }
        argv = &(vargv.front());
    }

    ~NidasAppArgv()
    {
        for (int i = 0; i < argc; ++i)
        {
            free(vargv[i]);
        }
    }

    std::vector<char*> vargv;
    char** argv;
    int argc;

private:

    NidasAppArgv(const NidasAppArgv&);
    NidasAppArgv& operator=(const NidasAppArgv&);

};



}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_NIDASAPP_H
