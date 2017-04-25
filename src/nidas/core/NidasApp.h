// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2016 UCAR, NCAR, All Rights Reserved
 ********************************************************************
*/
#ifndef NIDAS_CORE_NIDASAPP_H
#define NIDAS_CORE_NIDASAPP_H

#include "SampleTag.h"
#include "SampleMatcher.h"
#include <nidas/util/UTime.h>
#include <nidas/util/Socket.h>
#include <nidas/util/auto_ptr.h>

#include <string>
#include <list>
#include <set>

namespace nidas { namespace core {

class Project;

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
 * Convenience typedef for handling the command-line argv as a vector of
 * strings.
 **/
typedef std::vector<std::string> ArgVector;


/**
 * A NidasAppArg is command-line argument which can be handled by NidasApp.
 * The base class defines basic state and behavior about the argument, then
 * arguments can be subclassed from this class to provide extra
 * customization.  More standard arguments, shared by more than one nidas
 * app, are defined as members of the NidasApp class.  NIDAS applications
 * can add their own arguments.
 **/
class NidasAppArg
{
public:
    /**
     * Construct a NidasAppArg from a list of accepted short and long
     * flags, the syntax for any arguments to the flag, a usage string, and
     * a default value.  @p flags is a comma-separated list of the
     * command-line flags recognized this argument.  The usage string
     * describes the argument, something which can be printed as part of an
     * application's usage information.
     *
     * Example:
     *
     * This specifier shows the three forms that are accepted for an
     * argument.  The -l is obviously the short form, and it will not be
     * accepted if acceptShortFlag() is not true.  The other two are long
     * forms, each equivalent to the other.  If an option is deprecated,
     * that can be noted in the usage.  The default is "info".
     *
     * -l,--loglevel,--logconfig <logconfig> [info]
     * 
     * If an argument takes only a flag and no additional parameter, then
     * the syntax must be empty.
     *
     * Typically an application's arguments are instantiated as part of the
     * application's class, so they have the same lifetime as the
     * application instance and can be referenced to generate usage
     * information.  See NidasApp::enableArguments().
     *
     * When the application's arguments are parsed, then this argument 
     * is updated with the flag and value
     **/
    NidasAppArg(const std::string& flags,
                const std::string& syntax = "",
                const std::string& usage = "",
                const std::string& default_ = "");

    virtual
    ~NidasAppArg();

    /**
     * Set whether short flags are enabled or not.  Pass @p enable as false
     * to disable short flags and require only long flags instead.  By
     * default short flags are enabled.
     **/
    void
    acceptShortFlag(bool enable)
    {
        _enableShortFlag = enable;
    }

    /**
     * Add a flag which this argument should accept.  Use this to allow an
     * application to accept deprecated flags like -B and -E.
     **/
    void
    addFlag(const std::string& flag);

    /**
     * Completely replace the flags which this argument should accept.
     * This should be avoided if possible, otherwise the flags will not be
     * consistent across applications.  However, in some cases this is
     * necessary to remove a conflicting short flag.
     **/
    void
    setFlags(const std::string& flags);

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
     * Render the usage string for this particular argument, taking into
     * account which flags are enabled.  The returned string is formatted like
     * below, and always ends in a newline:
     *
     * <indent><flag>[,<flag>...] [<syntax>] [default: <default>]
     * <indent><indent>Description line one
     * <indent><indent>Description line two
     * ...
     **/
    std::string
    usage(const std::string& indent = "  ");

    /**
     * Return true if this argument has been filled in from a command-line
     * argument list, such as after a call to NidasApp::parseArgs().
     * If true, then this argument stores the flag that was recognized, and
     * also the value of any additional parameters to this argument.
     **/
    bool
    specified();

    /**
     * If this argument has been parsed from a command line list
     * (specified() returns true), then return the value passed after the
     * flag.  Otherwise return the default value.
     **/
    const std::string&
    getValue();

    /**
     * Return the command-line flag which this argument consumed.  For
     * example, if an argument which has multiple flags -l, --loglevel, and
     * --logconfig matches --loglevel, then getFlag() will return
     * --loglevel.
     **/
    const std::string&
    getFlag();

    /**
     * An argument is true if it is a stand-alone flag and was specified in
     * the arguments, or else if the flag value evaluates to true.
     **/
    bool
    asBool();

    /**
     * Parse the argument value as an integer, where the value could be the
     * default if no value has been explicitly parsed with parse().  Throw
     * NidasAppException if the whole value cannot be parsed as an integer.
     * See also asFloat().
     **/
    int
    asInt();

    /**
     * Same as asInt(), except parse the argument value as a float.
     **/
    float
    asFloat();

    /**
     * If argv[argi] matches this argument, then set the flag that was
     * found and also the value if this argument takes a value, and return
     * true.  Otherwise return false.  The vector is not modified, but if
     * argi is nonzero, then it is used as the starting index into argv,
     * and it is advanced according to the number of elements of argv
     * consumed by this argument.
     **/
    bool
    parse(const ArgVector& argv, int* argi = 0);

    /**
     * Return true if the given command-line @p flag matches one of this
     * argument's flags.
     **/
    bool
    accept(const std::string& flag);

    void
    setUsageString(const std::string& text)
    {
        _usage = text;
    }

    /**
     * Return the string of flags accepted by this NidasAppArg according to
     * the acceptShortFlag() setting.
     **/
    std::string
    getUsageFlags();

private:

    // Prevent the public NidasAppArg members of NidasApp from being
    // replaced with other arguments.
    NidasAppArg&
    operator=(const NidasAppArg&);
    NidasAppArg(const NidasAppArg&);

    std::string _flags;
    std::string _syntax;
    std::string _usage;
    std::string _default;
    std::string _arg;
    std::string _value;
    bool _enableShortFlag;

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
        NidasAppArg("", "input-url [...]"),
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


nidas_app_arglist_t
operator|(nidas_app_arglist_t arglist1, nidas_app_arglist_t arglist2);


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
<td>-s starttime</td><td>Skip samples before <em>start</em></td><td></td>
</tr>
<tr>
<td>-e endtime</td><td>Skip samples after <em>end</em></td><td></td>
</tr>
<tr>
<td>-d</td><td>Run in debug mode instead of as a background daemon.</td><td></td>
</tr>
</table>
 *
 * Below are notes about the deprecated options and the standard options
 * made available through NidasApp.
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
 * ### --logconfig <config>, -l <config> ###
 *
 * The older option --loglevel, and the short version -l for the
 * applications which accept it, are an alias for the newer --logconfig
 * option.
 *
 * The log config string is a comma-separated list of LogConfig fields,
 * where the field names are level, tag, file, function, file, and line.
 * Additionally, a field can have the value 'enable' or 'disable' to set
 * the active flag accordingly for matching log contexts. All the settings
 * are combined into a single LogConfig and added to the current scheme.
 *
 * If a field does not have an equal sign and is not 'enable' or 'disable',
 * then it is interpreted as just a log level, compatible with what the
 * --loglevel,-l option supported.
 *
 * _loglevel_ can be a number or the name of a log level:
 *
 * 7=debug,6=info,5=notice,4=warn,3=err
 *
 * The default log level when a NidasApp is created is *info*.
 * 
 * 
 * ### --logshow ###
 *
 * Show log points as they are encountered while the code is running.  This
 * shows what log points would log messages if enabled by the logging
 * configuration, and it gives information about each log point that can be
 * used to enable only that log point.
 *
 * ### --logfields <fields> ###
 *
 * Set the log message fields which will be shown in log messages.  The
 * <fields> argument is passed to the LogScheme::setShowFields() method of
 * the current logging scheme.
 *
 * ### --logparam <param>=<value> ###
 *
 * Set a log parameter in the application log scheme.
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
 *
 * ### -d,--debug debug mode ###
 *
 * The dsm and dsm_server applications both support a debug mode which runs
 * in the foreground instead of as a daemon, by default sends all log
 * messages to standard output.  The long form is --debug.
 *
 * ### -u,--user <username> ###
 *
 * For daemon applications, switch to the given user after setting required
 * capabilities.
 *
 * ### -H,--host <hostname> ###
 *
 * Run as if on the given host instead of using current system hostname.
 *
 **/
class NidasApp
{
public:

    /**
     * The four possible output formats for sensor-plus-sample IDs:
     *
     * auto - Use decimal for samples less than 0x8000, and hex otherwise.
     * decimal - Use decimal for all samples.
     * hex - Use hex for all samples.
     * octal - Use octal for all samples.  Not really used.
     *
     * The default is auto.  Set it with --id-format {auto|decimal|hex}.
     **/
    typedef enum idfmt { AUTO_ID, DECIMAL_ID, HEX_ID, OCTAL_ID } id_format_t;

    NidasAppArg XmlHeaderFile;
    NidasAppArg LogShow;
    NidasAppArg LogConfig;
    NidasAppArg LogFields;
    NidasAppArg LogParam;
    NidasAppArg Help;
    NidasAppArg ProcessData;
    NidasAppArg StartTime;
    NidasAppArg EndTime;
    NidasAppArg SampleRanges;
    NidasAppArg FormatHexId;
    NidasAppArg FormatSampleId;
    NidasAppArg Version;
    NidasAppInputFilesArg InputFiles;
    NidasAppArg OutputFiles;
    NidasAppArg Username;
    NidasAppArg Hostname;
    NidasAppArg DebugDaemon;

    /**
     * This is a convenience method to return all of the logging-related
     * options in a list, such that this list can be extended if new log
     * options are enabled.  Only LogConfig has a short log option of -l,
     * so if that option does not conflict, it should be safe to enable all
     * of them.  The current list contains these options: LogShow,
     * LogConfig, LogFields, LogParam.
     **/
    nidas_app_arglist_t
    loggingArgs();

    /**
     * Give the NidasApp instance a name, to be used for the usage info and
     * the logging scheme.  When a NidasApp is created, it replaces an
     * existing default logging scheme to set the default log level to
     * INFO.  If there is already a logging scheme without the default
     * name, then it will not be changed.
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
     * Set the name of this particular process, usually argv[0].  It
     * defaults to the application name passed to the constructor and
     * returned by getName().
     **/
    void
    setProcessName(const std::string& argv0);

    /**
     * Get the process name, as set by setProcessName(), or else return the
     * app name from getName().
     **/
    std::string
    getProcessName();

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
     * Add the list of NidasAppArg pointers to the set of arguments
     * accepted by this NidasApp instance.  Since the arguments are
     * referenced by pointer, their lifetime should match the lifetime of
     * the NidasApp instance.  For example, define additional non-standard
     * arguments as members of the same class in which NidasApp is a
     * member.
     *
     * @code
     * class MyApp {
     * NidasAppArg FixEverything;
     * NidasApp app;
     * }
     * ...
     * app.enableArguments(FixEverything);
     * @endcode
     *
     * The arglist can be created implicitly from the NidasAppArg
     * instances, as in this example:
     *
     * @code
     * NidasApp app('na');
     * app.enableArguments(app.LogConfig | app.ProcessData);
     * @endcode
     **/
    void
    enableArguments(const nidas_app_arglist_t& arglist);

    /**
     * Return the list of arguments which are supported by this NidasApp,
     * in other words, the arguments enabled by enableArguments().
     **/
    nidas_app_arglist_t
    getArguments()
    {
        return nidas_app_arglist_t(_app_arguments.begin(), _app_arguments.end());
    }

    /**
     * Call acceptShortFlag(false) on the given list of NidasApp instances,
     * meaning only the long flag will be recognized.
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
     * Set the list of command-line argument strings to be parsed and
     * handled by successive calls to nextArgument().  The usual calling
     * sequence is this:
     *
     * @code
     * app.enableArguments(...);
     * app.startArgs(ArgVector(argv+1, argv+argc));
     * NidasAppArg* arg;
     * while ((arg = app.parseNext())) {
     *    if (arg == &FixEverything)
     *        _fixeverything = True;
     *    else if (arg == &FixCount)
     *        _fixcount = FixCount.asInt();
     * }
     * @endcode
     **/
    void
    startArgs(const ArgVector& args);

    /**
     * Parse the next recognized argument from the list set in
     * startArgs().  If it is one of the standard arguments which are
     * members of NidasApp, then handle the argument also.  Either way,
     * fill in the NidasAppArg with the flag and value (if any) extracted
     * from the command-line, and return the pointer to that NidasAppArg.
     * If no more arguments are left or none are recognized, then return
     * null.  Raise NidasAppException if there is an error parsing any of
     * the standard arguments.
     * 
     * Recognized arguments are removed from the argument list.  Call
     * parseRemaining() to return the arguments which have not been parsed,
     * such as to access positional arguments or detect unrecognized
     * arguments.  Positional arguments like input sockets and file names
     * are not handled here, since they cannot be differentiated from
     * app-specific arguments yet.  Instead those arguments can be passed
     * explicitly to the parseInputs() method.
     **/
    NidasAppArg*
    parseNext() throw (NidasAppException);

    ArgVector
    unparsedArgs();

    /**
     * Parse all arguments accepted by this NidasApp in the command-line
     * argument list @p args, but only handle the standard arguments.  Any
     * application-specific arguments are only parsed and filled in.  This
     * is equivalent to calling startArgs() and then looping over
     * parseNext(), except the caller can not handle individual arguments
     * when they are parsed.  For some arguments this works fine, since the
     * last occurrence of an argument on the command-line will be the final
     * value returned by NidasAppArg::getValue().  Returns the remaining
     * unparsed arguments, same as would be returned by
     * unparsedArgs().
     *
     * The argument vector should not contain the process name.  So this is
     * a convenient way to call it from main():
     *
     * @code
     * ArgVector args = app.parseArgs(ArgVector(argv+1, argv+argc));
     * @endcode
     *
     * @param args Just the arguments, without the process name.
     * @returns Any remaining arguments not accepted by this NidasApp.
     **/
    ArgVector
    parseArgs(const ArgVector& args) throw (NidasAppException);

    /**
     * Convenience method to convert the (argv, argc) run string to a list
     * of arguments to pass to parseArgs().  Also, if the process name has
     * not been set with setProcessName(), then set it to argv[0].
     **/
    ArgVector
    parseArgs(int argc, const char* const argv[]) throw (NidasAppException);

    /**
     * Parse a LogConfig from the given argument using the LogConfig string
     * syntax, and add that LogConfig to the current LogScheme.
     **/
    void
    parseLogConfig(const std::string& optarg) throw (NidasAppException);

    /**
     * This is an alias for parseLogConfig(), since the LogConfig string
     * syntax is backwards compatible with parsing just log levels.
     **/
    void
    parseLogLevel(const std::string& optarg) throw (NidasAppException)
    {
        parseLogConfig(optarg);
    }

    nidas::util::UTime
    parseTime(const std::string& optarg);

    void
    parseUsername(const std::string& username);

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

    /**
     * Return LogScheme::logLevel() for the current log scheme.
     **/
    int
    logLevel();

    /**
     * Reset the logging configuration to the NidasApp default, meaning any
     * current log scheme is cleared of configs and the default scheme is
     * set to log level INFO.
     **/
    void
    resetLogging();

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

    std::string
    getConfigsXML();

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
     * Set the global interrupted state for NidasApp.  The default signal
     * handler sets this flag, but this method allows the flag to be set
     * from custom signal handlers or from other parts of the application.
     **/
    static void
    setInterrupted(bool interrupted);

    /**
     * Return a pointer to the application-wide Project instance.  The lifetime
     * is tied to this NidasApp instance.
     **/
    Project*
    getProject();

    /**
     * Return a usage string describing the arguments accepted by this
     * application, rendering each argument by calling NidasAppArg::usage()
     * with the given indent string.
     **/
    std::string
    usage(const std::string& indent = "  ");

    /**
     * Setup signal handling for HUP, INT, and TERM, equivalent to
     * calling addSignal() for each.
     **/
    static void
    setupSignals(void (*callback)(int signum) = 0);

    /**
     * Add the given signal @p signum to the list of signals handled by the
     * NidasApp signal handler, and optionally set an additional callback.
     * If non-zero, @p callback is a function which will be called when the
     * application receives an interrupt signal.  The callback is called
     * asynchronously, directly from the NidasApp signal handler function.
     * There can only be one callback, so the one in effect is whatever was
     * set last in setupSignals() and addSignal().  The NidasApp handler
     * sets the flag returned by interrupted(), and then calls the @p
     * callback function with the signal number as a parameter. @p signum
     * is a symbol like SIGALRM, SIGHUP, SIGINT, and SIGTERM from signal.h.
     * @p signum can be zero, in which case only the callback function
     * changes.
     *
     * If @p nolog is true, then nothing will be logged to cerr by the
     * default signal handler when that signal is received.
     **/
    static void
    addSignal(int signum, void (*callback)(int signum) = 0, bool nolog=false);

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

    /**
     * Write the sensor-plus-sample ID part of @p sid to stream @p out
     * using the format specified in @p idfmt.  This does not print the DSM
     * ID, only the SPS ID.
     **/
    static
    std::ostream&
    formatSampleId(std::ostream& out, id_format_t idfmt, dsm_sample_id_t sid);

    /**
     * Write the sensor-plus-sample ID part of @p sid to @p out using the
     * ID format of this NidasApp instance, the format returned by
     * getIdFormat(), same as calling formatSampleId(out, getIdFormat(),
     * sid).
     **/
    std::ostream&
    formatSampleId(std::ostream& out, dsm_sample_id_t sid);

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

    /**
     * Return the hostname passed to the Hostname argument, if any,
     * otherwise return the current hostname as returned by gethostname().
     **/
    std::string
    getHostName();

    /**
     * Return the username passed to the Username argument, if enabled,
     * otherwise return an empty string.
     **/
    std::string
    getUserName()
    { 
        return _username;
    }

    /**
     * Return the userid of the username passed to the Username argument,
     * otherwise 0.
     **/
    uid_t
    getUserID()
    {
        return _userid;
    }

    /**
     * Return the groupid of the username passed to the Username argument,
     * otherwise 0.
     **/
    uid_t
    getGroupID()
    {
        return _groupid;
    }

    /**
     * Setup a process with the user and group specified by the Username
     * argument, and attempt to set related capabilities.
     **/
    void
    setupProcess();
    
    /**
     * Switch this process to daemon mode unless the DebugDaemon argument
     * has been enabled.
     **/
    void
    setupDaemon();

    /**
     * Attempt mlockall() for this process, first adding the CAP_IPC_LOCK
     * capability if possible.
     **/
    void
    lockMemory();

    /**
     * If DebugDaemon argument is true, then this method does nothing.
     * Otherwise, create a pid file for this process and return 0.  If the
     * pid file already exists, then return 1.  PID files are created in
     * directory /tmp/run/nidas, which is itself created if necessary.
     **/
    int
    checkPidFile();

private:

    static NidasApp* application_instance;

    std::string _appname;

    std::string _argv0;

    bool _processData;

    std::string _xmlFileName;

    bool _idFormat_set;
    enum idfmt _idFormat;

    SampleMatcher _sampleMatcher;

    nidas::util::UTime _startTime;
 
    nidas::util::UTime _endTime;

    std::list<std::string> _dataFileNames;

    nidas::util::auto_ptr<nidas::util::SocketAddress> _sockAddr;

    std::string _outputFileName;
    int _outputFileLength;

    bool _help;

    std::string _username;

    std::string _hostname;

    uid_t _userid;

    gid_t _groupid;

    bool _deleteProject;

    std::set<NidasAppArg*> _app_arguments;

    ArgVector _argv;
    int _argi;
};


/**
 * Convert vector<string> args to dynamically allocated (argc, argv) pair
 * which will be freed when the instance is destroyed.  This is useful for
 * passing leftover NidasApp command-line arguments to getopt() functions:
 *
 * @code
 * NidasApp app('data_dump');
 * // Parse standard arguments and leave the rest.
 * ArgVector args = app.parseArgs(ArgVector(argv, argv+argc));
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
