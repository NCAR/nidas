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
#include "Datasets.h"
#include <nidas/util/UTime.h>
#include <nidas/util/Socket.h>
#include <nidas/util/auto_ptr.h>

#include <string>
#include <list>



namespace nidas { namespace core {

class Project;
class FileSet;
class SampleOutputBase;

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
 * Lists of arguments can be manipulated together by putting them into this
 * container type.  The container can be generated using operator|().
 **/
typedef std::vector<NidasAppArg*> nidas_app_arglist_t;

/**
 * Convenience typedef for handling the command-line argv as a vector of
 * strings.
 **/
typedef std::vector<std::string> ArgVector;

inline std::string
expectArg(const ArgVector& args, int i)
{
    if (i < (int)args.size())
    {
        return args[i];
    }
    throw NidasAppException("expected argument for option " + args[i-1]);
}

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

    enum ATTS: unsigned int { NONE=0, OMITBRIEF=2 };

    /**
     * Construct a NidasAppArg from a list of accepted short and long
     * flags, the syntax for any arguments to the flag, a usage string, and
     * a default value.  @p flags is a comma-separated list of the
     * command-line flags recognized by this argument.  The usage string
     * describes the argument, something which can be printed as part of an
     * application's usage information.
     *
     * The @p flags specifier can include multiple flags that are accepted
     * for an argument, separated by commas.  In the example below, -l is
     * obviously the short form, and it will not be accepted if
     * acceptShortFlag() is not true.  The others are long forms, each
     * equivalent to the other.  Deprecated options can be surrounded by
     * brackets.  If an option is deprecated, it will still be accepted on
     * the command-line, but it will not be shown in the usage. (Probably
     * it should be documented as deprecated in the usage string.)
     *
     * -l,--log[,--loglevel,--logconfig]
     * 
     * If an argument is only a flag and no additional parameter, then the
     * syntax must be empty, and the default value is assumed to be boolean
     * false.  When the flag is parsed in the arguments, then the value
     * will be true.  If a default boolean value is specified for a flag
     * with no parameters, then as a special case for long arguments, a
     * long form can be prefixed with --no- to set the value to false.
     * Thus a boolean option can be given an explicit default value by
     * passing "true" or "false" as the default value, and then it can be
     * set to "true" with the normal flag and set to "false" using the --no
     * form.
     *
     * Typically an application's arguments are instantiated as part of the
     * application's class, so they have the same lifetime as the
     * application instance and can be referenced to generate usage
     * information.  See NidasApp::enableArguments().
     *
     * When the application's arguments are parsed, then this argument 
     * is updated with the exact flag and value that set it.
     *
     * Arguments can be marked as required, which allows unset required
     * arguments to be detected by NidasApp::checkRequiredArguments().  See
     * setRequired().
     *
     * The @p atts parameter is a bitmask of attributes for the argument.
     * NidasApp::usage() will not show usage for arguments with the OMITBRIEF
     * attribute.  See omitBrief().
     **/
    NidasAppArg(const std::string& flags,
                const std::string& syntax = "",
                const std::string& usage = "",
                const std::string& default_ = "",
                bool required = false,
                unsigned int atts = NONE);

    virtual
    ~NidasAppArg();

    /**
     * Set whether this argument is required.  A required argument must be
     * supplied on the command line even if it has a default value.  If an
     * argument is not required because it has a default value, then an
     * application should not set that argument to be required.
     *
     * Defaults to true, but the client may disable this option by setting
     * the argument to false.
     **/
    void
    setRequired(bool isRequired=true);

    /**
     * Return whether this argument is required. See setRequired().
     **/
    bool
    isRequired();

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
     * below, each line prefixed with @p indent,  and always ends in a
     * newline.  If * @p brief is true, the usage lines are omitted.
     *
     * <indent><flag>[,<flag>...] [<syntax>] [default: <default>]
     * <indent><indent>Description line one
     * <indent><indent>Description line two
     * ...
     **/
    std::string
    usage(const std::string& indent = "  ", bool brief = false);

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
     * example, if an argument with multiple flags is matched, then
     * getFlag() returns the flag that matched the argument.
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
     * consumed by this argument.  This method is virtual so subclasses can
     * implement customized parsing, such as optionally consuming more than
     * one argument following a flag.
     **/
    virtual bool
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

    void
    setDefault(const std::string& dvalue)
    {
        _default = dvalue;
    }

    std::string
    getDefault()
    {
        return _default;
    }

    bool
    omitBrief()
    {
        return _omitbrief;
    }

protected:

    std::string _flags;
    std::string _syntax;
    std::string _usage;
    std::string _default;
    std::string _arg;
    std::string _value;
    bool _enableShortFlag;
    bool _required;
    bool _omitbrief;

    /**
     * Return true for arguments which are only a single argument.  They
     * are a single command-line flag with no following value, and
     * typically implying a boolean value.  This is equivalent to not
     * specifying a syntax when the argument is created.
     **/
    bool
    single();

private:

    // Prevent the public NidasAppArg members of NidasApp from being
    // replaced with other arguments.
    NidasAppArg&
    operator=(const NidasAppArg&);
    NidasAppArg(const NidasAppArg&);

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

    /**
     * Set a default input.  Typically @p spec is a default hostname or
     * address, and a default port can be passed in @p default_port.  Set
     * just a default port by passing an empty string in @p spec, in which
     * case there is no valid default input, but a socket input string
     * specified without a port (like sock:localhost) will use the default
     * port.
     **/
    void
    setDefaultInput(const std::string& spec, int port=0)
    {
        default_input = spec;
        if (port)
            default_port = port;
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


/**
 * Combine two arglists into a single arglist without any duplicates.
 * Order is preserved, with all arguments in the the second arglist
 * succeeding the args in the first arglist.
 **/
nidas_app_arglist_t
operator|(nidas_app_arglist_t arglist1, nidas_app_arglist_t arglist2);


/**
 * NidasApp handles common options for NIDAS applications.  The application
 * specifies which of the options are valid and it can specify defaults if
 * necessary, then command-line arguments can be parsed and their settings
 * retrieved through this instance.  This class allows for consistent option
 * letters and syntax across NIDAS applications.  Applications can extend
 * this class with their own options, either as a subclass or by delegation
 * to an instance.
 *
 * There are several NIDAS applications with common options that can be
 * consolidated in this class:
 *
 * - data_dump
 * - data_stats
 * - nidsmerge
 * - sensor_extract
 * - statsproc (uses -B and -E for time range)
 * - prep (uses -B and -E for time range)
 * - data_nc
 * - dsm (esp logging)
 * - dsm_server (esp logging)
 * - sync_server
 * - nimbus
 *
 * The goal is to keep syntax and meaning for common arguments consistent
 * across applications.
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
<tr>
<td>-f</td><td>Filter bad samples according to default filter rules.</td><td></td>
</tr>
<tr>
<td>--filter rules</td>
<td>Specify rules for filtering bad samples, using a key=value syntax.</td>
<td>--filter maxdsm=1024,mindsm=1,maxlen=32768,minlen=1</td>
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
 * ### --log <config>, -l <config> ###
 *
 * The older --loglevel and --logconfig are obsolete.  The short version
 * -l, for the applications which accept it, is an alias for the newer
 * --log option.
 *
 * The log config string is a comma-separated list of LogConfig fields,
 * where the field names are level, tag, file, function, file, and line.
 * Additionally, a field can have the value 'enable' or 'disable' to set
 * the active flag accordingly for matching log contexts. All the settings
 * are combined into a single LogConfig and added to the current scheme.
 *
 * If a field does not have an equal sign and is not 'enable' or 'disable',
 * then it is interpreted as just a log level, compatible with what the -l
 * option has always supported.
 *
 * _loglevel_ can be a number or the name of a log level:
 *
 * 7=debug,6=info,5=notice,4=warn,3=err
 *
 * The default log level when a NidasApp is created is *info*.
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
 * in the foreground instead of as a daemon, and by default sends all log
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
 * ## Logging ##
 *
 * When a NidasApp is constructed, it sets up its own default logging scheme
 * with a fallback log level of INFO.  That log scheme is set as the current
 * scheme only if there is not already a scheme with a non-empty name.  That
 * allows an app to install its own default LogScheme before creating a
 * NidasApp, but the app of course can also install its own scheme after
 * creating NidasApp.  When NidasApp parses logging arguments, it updates
 * and explicitly installs its LogScheme, so the user arguments always
 * override the app and NidasApp defaults.
 *
 * Since the default INFO log level is a fallback, that will be replaced by
 * the first log config provided as an argument.  So a log argument of
 * "warning" will set the log scheme to only show messages at or more
 * critical than warnings.
 **/
class NidasApp
{
public:

    /**
     * The four possible output formats for sensor-plus-sample IDs:
     *
     * noformat - Use an existing setting or the default.
     * auto - Use decimal for samples less than 0x8000, and hex otherwise.
     * decimal - Use decimal for all samples.
     * hex - Use hex for all samples.
     * octal - Use octal for all samples.  Not really used.
     *
     * The default is auto.  Set it with --id-format {auto|decimal|hex}.
     **/
    typedef enum {
        NOFORMAT_ID, AUTO_ID, DECIMAL_ID, HEX_ID, OCTAL_ID
    } id_format_t;

    /**
     * An IdFormat specifies the format for the SPS ID plus other
     * characteristics, like the width when using decimal format.  A width
     * of zero means use an existing setting, if any, otherwise use the
     * default.
     **/
    class IdFormat
    {
    public:
        IdFormat(id_format_t idfmt=NOFORMAT_ID) :
            _idFormat(idfmt),
            _width(0)
        {
        }
            
        IdFormat(const IdFormat& right) :
            _idFormat(right._idFormat),
            _width(right._width)
        {
        }

        IdFormat&
        setDecimalWidth(int width)
        {
            _width = width;
            return *this;
        }

        IdFormat&
        operator=(const IdFormat& right)
        {
            if (right._idFormat != NOFORMAT_ID)
                _idFormat = right._idFormat;
            if (right._width != 0)
                _width = right._width;
            return *this;
        }

        int
        decimalWidth()
        {
            if (_width)
                return _width;
            return 6;
        }

        id_format_t
        idFormat()
        {
            if (_idFormat != NOFORMAT_ID)
                return _idFormat;
            return AUTO_ID;
        }

        bool
        unset()
        {
            return (_idFormat == NOFORMAT_ID && _width == 0);
        }

        id_format_t _idFormat;
        int _width;
    };

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
    NidasAppArg ConfigsArg;
    NidasAppArg DatasetName;
    NidasAppArg Clipping;
    NidasAppArg SorterLength;
    NidasAppArg Precision{"--precision", "ndigits",
                          "Number of digits in floating point data values.  "
                          "Default 0 means 5 for floats, 10 for doubles",
                          "0"};

    /**
     * It is not enough to enable this arg in an app, the app must must
     * call checkPidFile() as well.
     **/
    NidasAppArg PidFile;

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
     * Construct a NidasApp instance and give it a name to be used for the
     * usage info.
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
     * Set whether unrecognized flags should throw an exception or not.  By
     * default, NidasApp throws an exception when parsing an argument which
     * starts with '-' but is not accepted by one of the enabled arguments.
     * Pass true to this method to ignore those unrecognized flags and
     * leave them in the unparsed arguments list.
     **/
    void
    allowUnrecognized(bool allow);

    /**
     * Return whether unrecognized flags are allowed or not.
     **/
    bool
    allowUnrecognized();

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
     * Add arguments to this NidasApp same as enableArguments() but also
     * set them as required.  See NidasAppArg::setRequired().  An
     * application can conveniently check whether required arguments have
     * been set by calling checkRequiredArguments() when all arguments have
     * been parsed.
     **/
    void
    requireArguments(const nidas_app_arglist_t& arglist);

    /**
     * Verify that all NidasAppArg arguments required by this NidasApp have
     * been specified, otherwise throw NidasAppException.
     **/
    void
    checkRequiredArguments();

    /**
     * Return the list of arguments which are supported by this NidasApp,
     * in other words, the arguments enabled by enableArguments() or
     * requireArguments().
     **/
    nidas_app_arglist_t
    getArguments();

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
     * handled by successive calls to nextArg().  The usual calling
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
     * Convenience method to convert the (argc, argv) run string to a list
     * of arguments to pass to startArgs().  Also, if the process name has
     * not been set with setProcessName(), then set it to argv[0].
     *
     * @throws NidasAppException
     **/
    void
    startArgs(int argc, const char* const argv[]);

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
     * unparsedArgs() to return the arguments which have not been parsed,
     * such as to access positional arguments or detect unrecognized
     * arguments.  Positional arguments like input sockets and file names
     * are not handled here, since they cannot be differentiated from
     * app-specific arguments yet.  Instead those arguments can be passed
     * explicitly to the parseInputs() method.
     *
     * @throws NidasAppException
     **/
    NidasAppArg*
    parseNext();

    /**
     * If the next argument to be parsed does not start with '-', then copy
     * it into @p arg and remove it from the arguments.  Callers can use
     * this to consume multiple arguments for a single option flag.
     * Returns true when the argument exists and has been copied into @p
     * arg, otherwise returns false and @p arg is unchanged.  The returned
     * argument will not be seen by parseNext(), and it will not be
     * returned by unparsedArgs().
     **/
    bool
    nextArg(std::string& arg);

    /**
     * Return any arguments left in the argument list.  From the argument list
     * passed into startArgs(), this returns the arguments which have not
     * already been handled by parseNext() or nextArg().  
     */
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
     * This method deliberately does not call checkRequiredArguments(),
     * since arguments like --help need to be handled by the caller even
     * when required arguments are legitimately missing from the argument
     * list.  Instead, applications with required arguments should
     * explicitly call checkRequiredArguments() after all required
     * arguments should have been parsed.
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
     * @throws NidasAppException
     **/
    ArgVector
    parseArgs(const ArgVector& args);

    /**
     * Convenience method to convert the (argc, argv) run string to a list
     * of arguments to pass to parseArgs().  Also, if the process name has
     * not been set with setProcessName(), then set it to argv[0].
     *
     * @throws NidasAppException
     **/
    ArgVector
    parseArgs(int argc, const char* const argv[]);

    /**
     * Parse a LogConfig from the given argument using the LogConfig string
     * syntax, and add that LogConfig to the current LogScheme.
     *
     * @throws NidasAppException
     **/
    void
    parseLogConfig(const std::string& optarg);

    /**
     * This is an alias for parseLogConfig(), since the LogConfig string
     * syntax is backwards compatible with parsing just log levels.
     *
     * @throws NidasAppException
     **/
    void
    parseLogLevel(const std::string& optarg)
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
     *
     * @throws NidasAppException
     **/
    void
    parseInputs(const std::vector<std::string>& inputs,
                std::string default_input = "",
                int default_port = 0);

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
     *
     * @throws NidasAppException
     **/
    void
    parseOutput(const std::string& optarg);

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
     * Reset logging to the NidasApp default.  This erases all log schemes,
     * then sets up the default NidasApp log scheme same as happens in the
     * NidasApp constructor.
     **/
    void
    resetLogging();

    /**
     * Set the process-data setting in this NidasApp.
     **/
    void
    setProcessData(bool process)
    {
        _processData = process;
    }

    /**
     * Return the current process-data setting for this NidasApp.
     **/
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

    /**
     * Derive the path to the XML file which lists project configs.  This uses
     * standard paths parameterized by environment variables, and accepts the
     * first path whose environment variables are set in the environment:
     *
     *  - "$NIDAS_CONFIGS"
     *  - "$PROJ_DIR/$PROJECT/$AIRCRAFT/nidas/flights.xml"
     *  - "$ISFS/projects/$PROJECT/ISFS/config/configs.xml"
     *  - "$ISFF/projects/$PROJECT/ISFF/config/configs.xml"
     *
     * If none of the above paths can be derived because of missing
     * environment variables, then throw InvalidParameterException.
     **/
    std::string
    getConfigsXML();

    /**
     * Derive a path to an XML datasets file according to the current
     * environment settings, searching these paths in order:
     *
     *  - "$ISFS/projects/$PROJECT/ISFS/config/datasets.xml"
     *  - "$ISFF/projects/$PROJECT/ISFF/config/datasets.xml"
     *
     * Parse the derived file and return from it the Dataset with the given
     * name.  Throws an exception if the Dataset cannt be loaded.
     *
     * @throws nidas::util::InvalidParameterException
     * @throws XMLException
     **/
    nidas::core::Dataset
    getDataset(const std::string& datasetname);

    /**
     * @brief Return the StartTime as a UTime.
     *
     * The value is UTime::MIN unless set by the StartTime argument.
     *
     * @return nidas::util::UTime 
     */
    nidas::util::UTime
    getStartTime()
    {
        return _startTime;
    }

    /**
     * @brief Return the EndTime as a UTime.
     *
     * The value is UTime::MAX unless set by the EndTime argument.
     *
     * @return nidas::util::UTime 
     */
    nidas::util::UTime
    getEndTime()
    {
        return _endTime;
    }

    /**
     * If Clipping has been enabled, call setTimeClippingWindow() on the given
     * @p output using @p start and @p end.
     */
    void
    setOutputClipping(const nidas::util::UTime& start,
                      const nidas::util::UTime& end,
                      nidas::core::SampleOutputBase* output);

    void
    setFileSetTimes(const nidas::util::UTime& start,
                    const nidas::util::UTime& end,
                    nidas::core::FileSet* fset);

    /**
     * Return the value of the global interrupted flag.  If @p
     * allow_exception is true, and an exception interruption has been set
     * with setException(), then this call will throw that exception.  This
     * allows code which checks NidasApp::interrupted() in a loop to work
     * the usual way but also propagate exceptions from the main loop when
     * they happen elsewhere in the application.
     **/
    bool
    interrupted(bool allow_exception=true);

    /**
     * Set the global interrupted state for NidasApp.  The default signal
     * handler sets this flag, but this method allows the flag to be set
     * from custom signal handlers or from other parts of the application.
     **/
    static void
    setInterrupted(bool interrupted);

    /**
     * Like setInterrupted(true), but also allows an exception to be stored
     * which can be tested and retrieved later with hasException() and
     * getException().  This is useful, for example, in sample receive()
     * methods which may be called from multiple threads and not
     * necessarily from the main thread where readSamples() is called.
     *
     * Note the exception type is strictly nidas::util::Exception, so a
     * subclass reference can be passed, but the subclass is stripped when
     * copied by NidasApp.
     **/
    void
    setException(const nidas::util::Exception& ex);

    /**
     * See setException().
     **/
    bool
    hasException();

    /**
     * Return the current exception.  If no exception has been set
     * (hasException() returns false), then the return value is just a
     * default constructed nidas::util::Exception.
     **/
    nidas::util::Exception
    getException();

    /**
     * Return a pointer to the application-wide Project instance.  The lifetime
     * is tied to this NidasApp instance.
     **/
    Project*
    getProject();

    /**
     * Return a usage string describing the arguments accepted by this
     * application, rendering each argument by calling NidasAppArg::usage()
     * with the given @p indent string and @p brief parameter.
     **/
    std::string
    usage(const std::string& indent, bool brief);

    /**
     * Return the usage string, formatted according to the current
     * brief setting.
     */
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
     * Return true if brief help requested, such as with -h.
     */
    bool
    briefHelp()
    {
        return _brief;
    }

    /**
     * Store the format in which sample IDs should be shown.
     **/
    void
    setIdFormat(IdFormat idt);

    IdFormat
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
    formatSampleId(std::ostream& out, IdFormat idfmt, dsm_sample_id_t sid);

    /**
     * Write the sensor-plus-sample ID part of @p sid to @p out using the
     * ID format of this NidasApp instance, the format returned by
     * getIdFormat(), same as calling formatSampleId(out, getIdFormat(),
     * sid).
     **/
    std::ostream&
    formatSampleId(std::ostream& out, dsm_sample_id_t sid);

    /**
     * A more convenient form of formatSampleId() which just returns a
     * string, and also includes the DSM id in the familiar format
     * "<dsmid>,<sid>".
     **/
    std::string
    formatId(dsm_sample_id_t sid);

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
     * Like getHostName(), but any '.' qualifiers are removed and only the
     * unqualified name is returned.
     **/
    std::string
    getShortHostName();

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
     * @brief Setup logging according to daemon mode.
     *
     * Setup the logging scheme and create a Logger according to the daemon
     * mode.  Usually an application just calls setupDaemon(), since that
     * method calls setupDaemonLogging() before switching to a daemon (if
     * not disabled).
     **/
    void
    setupDaemonLogging(bool daemon_mode);

    /**
     * Call setupDaemonLogging(daemon_mode), where daemon_mode is true only
     * if DebugDaemon is false.
     */
    void
    setupDaemonLogging();

    /**
     * Call setupDaemonLogging(), then switch this process to daemon mode
     * if @p daemon_mode is true.
     **/
    void
    setupDaemon(bool daemon_mode);

    /**
     * Call setupDaemon(daemon_mode), where daemon_mode is true only if
     * DebugDaemon is false.
     */
    void
    setupDaemon();

    /**
     * Attempt mlockall() for this process, first adding the CAP_IPC_LOCK
     * capability if possible.
     **/
    void
    lockMemory();

    /**
     * Create a pid file for this process and return 0.  If the pid file
     * already exists, then return 1.  The path to the PID file is set by
     * the PidFile NidasAppArg, which defaults to
     * /tmp/run/nidas/<appname>.pid.  The path can be changed on the
     * command-line if the PidFile argument has been enabled.  The
     * directory for the pid file is created if necessary.  Usually an app
     * calls this method after the setupDaemon() and setupProcess() calls.
     * This method checks for the pid file even if DebugDaemon is enabled.
     * Otherwise it is possible to create multiple instances of a nidas
     * service on a host which interfere with each other.  If it is
     * necessary to start multiple processes, then the --pid argument must
     * be used change the pid file path.
     **/
    int
    checkPidFile();

    /**
     * @brief Return sorter length value, or throw std::invalid_argument.
     *
     * This checks the sorter length against the given min and max after
     * parsing it as a float.  Throws NidasAppException if the value is out of
     * range, otherwise returns the value.
     */
    float
    getSorterLength(float min, float max);

private:

    void
    setupLogScheme();

    static NidasApp* application_instance;

    std::string _appname;

    std::string _argv0;

    bool _processData;

    std::string _xmlFileName;

    IdFormat _idFormat;

    SampleMatcher _sampleMatcher;

    nidas::util::UTime _startTime;
 
    nidas::util::UTime _endTime;

    std::list<std::string> _dataFileNames;

    nidas::util::auto_ptr<nidas::util::SocketAddress> _sockAddr;

    std::string _outputFileName;
    int _outputFileLength;

    bool _help;
    bool _brief;

    std::string _username;

    std::string _hostname;

    uid_t _userid;

    gid_t _groupid;

    bool _deleteProject;

    nidas_app_arglist_t _app_arguments;

    ArgVector _argv;
    int _argi;

    bool _hasException;
    nidas::util::Exception _exception;

    bool _allowUnrecognized;

    /// NidasApp keeps track of the LogScheme built up from arguments.
    nidas::util::LogScheme _logscheme;
};


/**
 * Convert vector<string> args to dynamically allocated (argc, argv) pair
 * which will be freed when the instance is destroyed.  This is useful for
 * passing leftover NidasApp command-line arguments to getopt() functions.
 * The argv array includes the process name, as expected by getopt().
 *
 * @code
 * NidasApp app('data_dump');
 * // Parse standard arguments and leave the rest.
 * ArgVector args = app.parseArgs(ArgVector(argv, argv+argc));
 * NidasAppArgv left(argv[0], args);
 * int opt_char;
 * while ((opt_char = getopt(left.argc, left.argv, "...")) != -1) {
 *    ...
 * }
 * @endcode
 **/
struct NidasAppArgv
{
    NidasAppArgv(const std::string& argv0,
                 const std::vector<std::string>& args) :
        vargv(), argv(0), argc(0)
    {
        vargv.push_back(strdup(argv0.c_str()));
        for (unsigned int i = 0; i < args.size(); ++i)
        {
            vargv.push_back(strdup(args[i].c_str()));
        }
        argv = &(vargv.front());
        argc = (int)vargv.size();
    }

    /**
     * Given the opt index after getopt() finishes, return a vector of any
     * remaining arguments, suitable for passing to
     * NidasApp::parseInputs().
     **/
    ArgVector
    unparsedArgs(int optindex)
    {
        return std::vector<std::string>(vargv.begin()+optindex, vargv.end());
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
