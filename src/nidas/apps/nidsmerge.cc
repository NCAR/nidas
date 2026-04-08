// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

/**
 * Merge and sort samples from one or more sets of NIDAS archive files
 * into a new set of output files.  This is typically done for ISFS
 * projects, where an archive is kept locally on each DSM, and the
 * raw data are also sent in real-time via UDP (nidas_udp_relay) to a server.
 * The merge should fill holes in the data caused by failures of a
 * local disk on a DSM or due to the normal loss of data over UDP.
 *
 * June 2025: add correction for non-increasing sample times,
 * keeping track of the previous sample time for each input and
 * sensor (sample id). If a non-increasing sample time
 * is found, adjust its time tag to the last sample time from
 * that sensor plus 1 microsecond.
 *
 * This is much the same correction applied in real-time by
 * MessageStreamScanner to correct for possible non-increasing
 * times due to serial buffering: nidas commit 316aab17d4e, Dec 22 2018.
 *
 * This correction was added when re-merging the AHATS data
 * from a saved copy of the DSM and UDP raw data files.  It should
 * remove folds in the data that happen when sorting samples with
 * incorrect, non-increasing times. 
 *
 * Comparing the CSAT3 sample counter diagnostic in data processed
 * from merges created with and without this correction 
 * should provide a indication whether this correction improves
 * data quality. That is to be done...
 */

#include <nidas/core/FileSet.h>
#include <nidas/core/Bzip2FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/SortedSampleSet.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>
#include <nidas/core/NidasApp.h>
#include <nidas/core/BadSampleFilter.h>
#include <nidas/util/Logger.h>

#include <unistd.h>

#include <csignal>
#include <climits>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

using nidas::util::IOException;
using nidas::util::EOFException;
using nidas::util::UTime;



struct InputStats
{
    size_t nread{0};
    size_t nunique{0};
    size_t nnonincr{0};
    size_t nnonincr_free{0};

    InputStats& operator+=(const InputStats& other)
    {
        nread += other.nread;
        nunique += other.nunique;
        nnonincr += other.nnonincr;
        nnonincr_free += other.nnonincr_free;
        return *this;
    }
};


struct InputInfo
{
    SampleInputStream* stream{nullptr};
    dsm_time_t lastTime{LONG_LONG_MIN};
    // stats within a single readahead window
    InputStats window_stats{};
    // sum of stats over entire input
    InputStats total_stats{};
    bool eof{false};

    InputInfo(SampleInputStream* sis):
        stream(sis)
    {
    }

    InputInfo(const InputInfo& other) = default;
    InputInfo& operator=(const InputInfo& other) = default;
};


class NidsMerge: public HeaderSource
{
public:

    NidsMerge();

    int parseRunstring(int argc, char** argv);

    int run();

    int main(int argc, char** argv);

    int usage(const char* argv0);

    void sendHeader(dsm_time_t thead,SampleOutput* out);

    int numOpen() const
    {
        int nopen = 0;
        for (auto& i : inputs)
            nopen += !i.eof;
        return nopen;
    }

    void printStats(dsm_time_t tcur, bool totals=false);

private:

    void addInputStream(const list<string>& inputFiles);

    dsm_time_t flushSorter(dsm_time_t tcur, SampleOutputStream& outStream);

    vector<list<string> > inputFileNames;

    string outputFileName;

    vector<InputInfo> inputs;

    long long readAheadUsecs;

    UTime startTime;
 
    UTime endTime;

    int outputFileLength;

    SampleInputHeader header;

    string configName;

    list<unsigned int> allowed_dsms; /* DSMs to require.  If empty*/

    /*
    * SortedSampleSet2 does a sort by the full sample header -
    *      the timetag, sample id and the sample length.
    *      Subsequent Samples with identical headers but different
    *      data will be discarded.
    * SortedSampleSet3 sorts by the full sample header and
    *      then compares the data, keeping a sample if it has
    *      an identical header but different data.
    * SortedSampleSet3 will be less efficient at merging multiple
    * copies of an archive which contain many identical samples.
    * Set3 is necessary for merging TREX ISFF hotfilm data samples which
    * may have identical timetags on the 2KHz samples but different data,
    * since the system clock was not well controlled: used a GPS but no PPS.
    * TODO: create a SortedSampleSet interface, with the two implementations
    * and allow the user to choose Set2 or Set3 with a command line option.
    */
    SortedSampleSet3 sorter;

    // lastSampleTimes keeps track of the latest time for each combination of
    // input stream and sample id.  it is implemented as a map from the input,
    // id pair to the last time.
    typedef std::pair<SampleInputStream*, dsm_sample_id_t> InputSampleIdPair;
    typedef std::map<InputSampleIdPair, dsm_time_t> LastSampleTimesMap;
    LastSampleTimesMap lastSampleTimes{};

    bool getLastSampleTime(SampleInputStream* sis, dsm_sample_id_t sampleId,
                           dsm_time_t& lastTime);

    void setLastSampleTime(SampleInputStream* sis, dsm_sample_id_t sampleId,
                           dsm_time_t lastTime);

    unsigned long ndropped;

    NidasApp _app;

    BadSampleFilterArg FilterArg;
    NidasAppArg KeepOpening;
    NidasAppArg PrintHeader;
    NidasAppArg ForceIncreasingTimes{
        "--force-increasing-times", "",R"""(
When a sample has a time tag at or preceding the latest sample time for
that input stream and sample ID, force increasing sample times
by adding one microsecond, before inserting the sample into the output
sorter.  This should be an unusual requirement, since NIDAS normally enforces
increasing sample times when the samples are read from a sensor, thus
it is disabled by default.)"""
    };
};

bool
NidsMerge::
getLastSampleTime(SampleInputStream* sis, dsm_sample_id_t sampleId,
                  dsm_time_t& lastTime)
{
    InputSampleIdPair key(sis, sampleId);
    LastSampleTimesMap::const_iterator it = lastSampleTimes.find(key);
    if (it != lastSampleTimes.end()) {
            lastTime = it->second;
            return true;
    }
    return false;
}

void
NidsMerge::
setLastSampleTime(SampleInputStream* sis, dsm_sample_id_t sampleId,
                  dsm_time_t lastTime)
{
    lastSampleTimes[InputSampleIdPair(sis, sampleId)] = lastTime;
}


int main(int argc, char** argv)
{
    NidsMerge merge;
    return merge.main(argc, argv);
}

/* static */
int NidsMerge::usage(const char* argv0)
{
    cerr <<
    "Usage: " << argv0 << " [options] {-i input [...]} \n"
    "\n"
    "Options:\n"
              << _app.usage() <<
    "Example (from ISFF/TREX): \n\n" << argv0 <<
    "   -i /data1/isff_%Y%m%d_%H%M%S.dat \n"
    "    -i /data2/central_%Y%m%d_%H%M%S.dat\n"
    "    -i /data2/south_%Y%m%d_%H%M%S.dat\n"
    "    -i /data2/west_%Y%m%d_%H%M%S.dat\n"
    "    -o /data3/isff_%Y%m%d_%H%M%S.dat -l 14400 -r 10\n"
    "    -s \"2006 Apr 1 00:00\" -e \"2006 Apr 10 00:00\"\n";
    return 1;
}


int NidsMerge::main(int argc, char** argv)
{
    NidasApp::setupSignals();

    try {
        int res = parseRunstring(argc, argv);
        if (res != 0)
            return res;
        run();
    }
    catch (NidasAppException& ex)
    {
        cerr << ex.what() << endl;
        cerr << "Use -h to see usage info." << endl;
        return 1;
    }
    catch (IOException& ioe) {
        cerr << ioe.what() << endl;
        return 1;
    }
    return 0;
}


NidsMerge::NidsMerge():
    inputFileNames(),outputFileName(),inputs(),
    readAheadUsecs(30*USECS_PER_SEC),startTime(UTime::MIN),
    endTime(UTime::MAX), outputFileLength(0),header(),
    configName(), allowed_dsms(),
    sorter(),
    ndropped(0),
    _app("nidsmerge"),
    FilterArg(),
    KeepOpening
    ("--keep-opening", "",
     "Open the next file when an error occurs instead of stopping.",
     "true"),
    PrintHeader("--print-header", "",
                "Print the header whenever written to the output.")
{
}

void
check_fileset(list<string>& filenames, bool& requiretimes)
{
    bool timespecs = false;
    // Collect any additional filenames in the file set up
    // until the next option specified.
    for (list<string>::iterator it = filenames.begin();
         it != filenames.end(); ++it)
    {
        string& filespec = *it;
        if (filespec.find('%') != string::npos)
            timespecs = true;
    };
    if (timespecs && filenames.size() != 1)
    {
        std::ostringstream xmsg;
        xmsg << "Only one filespec allowed in a file set "
                << "with time specifiers : " << *filenames.begin()
                << " ... " << *filenames.rbegin();
        throw NidasAppException(xmsg.str());
    }
    requiretimes |= timespecs;
}


int NidsMerge::parseRunstring(int argc, char** argv)
{
    // Use LogConfig instead of the older LogLevel option to avoid -l
    // conflict with the output file length option.  Also the -i input
    // files option is specialized in that there can be one -i option for
    // each input file set, and multiple files can be added to an input
    // file set by passing multiple filenames after each -i.
    NidasApp& app = _app;

    NidasAppArg InputFileSet
        ("-i", "<filespec> [...]",
         "Create a file set from all <filespec> up until the next option.\n"
         "The file specifier is either filename pattern with time\n"
         "specifier fields like %Y%m%d_%H%M, or it is one or more\n"
         "filenames which will be read as a consecutive stream.");
    NidasAppArg InputFileSetFile
        ("-I", "<filespec>",
         "Read filesets from file <filespec>.  Each line in the given\n"
         "file is taken as the argument to a single -i option.  So\n"
         "each line is either a single filename pattern with time\n"
         "specifiers, or a list of files which will be read as a\n"
         "consecutive stream.");
    NidasAppArg ReadAhead
        ("-r,--readahead", "seconds",
         "How much time to read ahead and sort the input samples\n"
         "before outputting the sorted, merged samples.", "30");
    NidasAppArg ConfigName
        ("-c,--config", "configname",
         "Set the config name for the output header.\n"
         "This is different than the standard -x option which names\n"
         "the configuration to read.  nidsmerge does not read\n"
         "the XML configuration file, but it can set a new path\n"
         "in the output header which other nidas utilities can use.\n"
         "Example: -c $ISFF/projects/AHATS/ISFF/config/ahats.xml\n"
         "Any environment variable expansions should be single-quoted\n"
         "if they should not be replaced by the shell.");

    app.enableArguments(app.LogConfig | app.LogShow | app.LogFields |
                        app.LogParam | app.StartTime | app.EndTime |
                        app.OutputFiles | app.Clipping | KeepOpening |
                        FilterArg | InputFileSet | InputFileSetFile |
                        ReadAhead | ConfigName | app.OutputFileLength |
                        app.SampleRanges | PrintHeader |
                        ForceIncreasingTimes | app.Version | app.Help);
    // -i conflicts with input specifiers, so require --samples
    app.SampleRanges.acceptShortFlag(false);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = false;
    // -l conflicts with output file length.
    app.LogConfig.acceptShortFlag(false);

    bool requiretimes = false;
    app.startArgs(argc, argv);
    std::ostringstream xmsg;
    NidasAppArg* arg;
    // handle just the arguments which must be accumulated in the order
    // they occur on the command-line.
    while ((arg = app.parseNext()))
    {
        if (arg == &InputFileSet)
        {
            // First argument has already been retrieved.
            list<string> fileNames;
            string filespec = InputFileSet.getValue();
            // Collect any additional filenames in the file set up
            // until the next option specified.
            do {
                fileNames.push_back(filespec);
            } while (app.nextArg(filespec));
            check_fileset(fileNames, requiretimes);
            inputFileNames.push_back(fileNames);
        }
        else if (arg == &InputFileSetFile)
        {
            // filepath is the argument
            string path = arg->getValue();
            std::ifstream files(path.c_str());
            string line;
            while (!files.eof())
            {
                list<string> filenames;
                std::getline(files, line);
                DLOG(("Inputs line: ") << line);
                std::istringstream fileset(line);
                string filespec;
                while (fileset >> filespec)
                {
                    // skip lines whose first non-ws character is '#'
                    if (filespec[0] == '#')
                        break;
                    filenames.push_back(filespec);
                }
                // ignore empty lines
                if (filenames.size())
                {
                    check_fileset(filenames, requiretimes);
                    inputFileNames.push_back(filenames);
                }
            }
        }
    }
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    ArgVector unparsed = app.unparsedArgs();
    if (unparsed.size() > 0)
    {
        xmsg << "Unrecognized arguments:";
        for (auto& arg: unparsed)
            xmsg << " " << arg;
        throw NidasAppException(xmsg.str());
    }
    readAheadUsecs = ReadAhead.asInt() * (long long)USECS_PER_SEC;
    configName = ConfigName.getValue();
    startTime = app.getStartTime();
    endTime = app.getEndTime();

    app.validateOutput();
    outputFileName = app.outputFileName();
    outputFileLength = app.outputFileLength();

    if (requiretimes && (startTime.isMin() || endTime.isMax()))
    {
        xmsg << "Start and end times must be set when a fileset uses "
                << "a % time specifier.";
        throw NidasAppException(xmsg.str());
    }

    static nidas::util::LogContext configlog(LOG_DEBUG);
    if (configlog.active())
    {
        nidas::util::LogMessage msg(&configlog);
        msg << "nidsmerge options:\n"
            << "filter: " << FilterArg.getFilter() << "\n"
            << "readahead: " << readAheadUsecs/USECS_PER_SEC << "\n"
            << "configname: " << configName << "\n"
            << "start: " << startTime.format(true,"%Y %b %d %H:%M:%S") << "\n"
            << "  end: " << endTime.format(true,"%Y %b %d %H:%M:%S") << "\n"
            << "output: " << outputFileName << "\n"
            << "output length: " << outputFileLength << "\n";
        for (unsigned int ii = 0; ii < inputFileNames.size(); ii++) {
            msg << "input fileset:";
            const list<string>& inputFiles = inputFileNames[ii];
            list<string>::const_iterator fi = inputFiles.begin();
            for ( ; fi != inputFiles.end(); ++fi)
                msg << " " << *fi;
            msg << "\n";
        }
    }
    return 0;
}


void NidsMerge::sendHeader(dsm_time_t,SampleOutput* out)
{
    if (configName.length() > 0)
        header.setConfigName(configName);
    if (PrintHeader.asBool())
    {
            std::cout << header.toString() << std::endl;
    }
    header.write(out);
}


inline std::string
tformat(dsm_time_t dt)
{
    return UTime(dt).format(true, "%Y-%b-%d_%H:%M:%S.%6f");
}


typedef pair<string, size_t> header_t;

template<typename T>
void
print_column(const header_t& h, const T& val)
{
    cout << setw(h.second) << val << ' ';
}


void print_headers(std::ostream& out, const vector<header_t>& headers)
{
    for (auto& h : headers)
        out << setw(h.second) << h.first << ' ';
    out << endl;
}


void
find_preceding_samples(dsm_time_t tcur, SortedSampleSet3& sorter,
                       SortedSampleSet3::const_iterator& begin,
                       SortedSampleSet3::const_iterator& end)
{
    SampleT<char> dummy;
    SortedSampleSet3::const_iterator rsb = sorter.begin();
    // get iterator pointing at first sample equal to or greater
    // than dummy sample
    dummy.setTimeTag(tcur);
    SortedSampleSet3::const_iterator rsi = sorter.lower_bound(&dummy);
    begin = rsb;
    end = rsi;
}


/**
 * Free their references and remove the samples from sorter set for the given
 * range of iterators.  Return the number of samples removed.
 */
size_t remove_samples(SortedSampleSet3& sorter,
                      SortedSampleSet3::const_iterator rsb,
                      SortedSampleSet3::const_iterator rsi)
{
    // remove samples from sorted set
    size_t before = sorter.size();
    for (auto si = rsb; si != rsi; ++si)
    {
        const Sample *sample = *si;
        sample->freeReference();
    }
    if (rsi != rsb)
        sorter.erase(rsb, rsi);
    return before - sorter.size();
}


dsm_time_t
NidsMerge::flushSorter(dsm_time_t tcur,
                       SampleOutputStream& outStream)
{
    DLOG(("flushing sorter up to ") << tformat(tcur)
         << ", sorter size=" << sorter.size());

    // write out any samples which are before tcur but also clipped at the
    // endTime, if set.  then remove all the samples before tcur.
    SortedSampleSet3::const_iterator rsb, rsi;
    find_preceding_samples(tcur, sorter, rsb, rsi);
    size_t noutput = 0;
    dsm_time_t last_written = LONG_LONG_MIN;
    for (auto si = rsb; si != rsi; ++si)
    {
        const Sample *sample = *si;
        if (!endTime.isSet() || (sample->getTimeTag() < endTime.toUsecs()))
        {
            bool ok = outStream.receive(sample);
            if (!ok)
                throw IOException("send sample",
                    "Send failed, output disconnected.");
            noutput++;
            last_written = sample->getTimeTag();
        }
    }
    size_t nremoved = remove_samples(sorter, rsb, rsi);
    size_t after = sorter.size();
    size_t before = after + nremoved;

    static vector<header_t> headers = {
        {"flush output end time", 30},
        {"before", 8}, {"after", 8}, {"output", 8}, {"removed", 8}
    };
    print_headers(cout, headers);
    print_column(headers[0], tformat(tcur));
    print_column(headers[1], before);
    print_column(headers[2], after);
    print_column(headers[3], noutput);
    print_column(headers[4], nremoved);
    cout << endl;
    return last_written;
}

void
NidsMerge::addInputStream(const list<string>& inputFiles)
{
    nidas::core::FileSet* fset{nullptr};

    list<string>::const_iterator fi = inputFiles.begin();
    if (inputFiles.size() == 1 && fi->find('%') != string::npos)
    {
        fset = FileSet::createFileSet(*fi);
        // If a time range has been given, then presumably the intent is to
        // merge all samples within that time range, even if they appear in
        // the data files just outside the time range, so those need to be
        // included those also.  The Clipping argument enables the expansion.
        _app.setFileSetTimes(startTime, endTime, fset);
    }
    else
    {
        fset = nidas::core::FileSet::getFileSet(inputFiles);
    }
    fset->setKeepOpening(KeepOpening.asBool());

    DLOG(("adding input: ") << "name=" << fset->getName()
         << "; start=" << startTime.format(true, "%c")
         << "; end=" << endTime.format(true, "%c"));

    // SampleInputStream owns the iochan ptr.
    SampleInputStream* input = new SampleInputStream(fset);
    inputs.push_back(InputInfo(input));

    // Set the input stream filter in case other options were set
    // from the command-line that do not filter samples, like
    // skipping nidas input headers.
    BadSampleFilter& bsf = FilterArg.getFilter();
    bsf.setDefaultTimeRange(startTime, endTime);
    input->setBadSampleFilter(bsf);

    try {
        input->readInputHeader();
        // save header for later writing to output
        header = input->getInputHeader();
    }
    catch (const EOFException& e) {
        cerr << e.what() << endl;
        inputs.back().lastTime = LONG_LONG_MAX;
        inputs.back().eof = true;
    }
    // IOException propagates to here only on an io read error or if
    // keep-opening is false and a file cannot be opened.  either are good
    // reasons to exit with an error, so let the exception propagate.
}


void ageOffNonIncr(dsm_time_t tt, SortedSampleSet3& nonincr)
{
    DLOG(("aging off non-increasing samples up to ")
          << tformat(tt) << ", size=" << nonincr.size());
    SortedSampleSet3::const_iterator rsb, rsi;
    find_preceding_samples(tt, nonincr, rsb, rsi);
    size_t nremoved = remove_samples(nonincr, rsb, rsi);
    size_t before = nonincr.size() + nremoved;
    size_t after = nonincr.size();

    static vector<header_t> headers = {
        {"nonincr age-off time", 30},
        {"before", 8}, {"after", 8}, {"removed", 8}
    };
    print_headers(cout, headers);
    print_column(headers[0], tformat(tt));
    print_column(headers[1], before);
    print_column(headers[2], after);
    print_column(headers[3], nremoved);
    cout << endl;
}


void clearNonIncr(SortedSampleSet3& nonincr)
{
    remove_samples(nonincr, nonincr.begin(), nonincr.end());
}


Sample*
duplicateSample(const Sample* samp)
{
    // make a copy of the sample, preserving its data type
    Sample* dup = nidas::core::getSample(samp->getType(),
                                         samp->getDataByteLength());
    dup->setId(samp->getId());
    assert(dup->getDataByteLength() == samp->getDataByteLength());
    ::memcpy(dup->getVoidDataPtr(), samp->getConstVoidDataPtr(),
             samp->getDataByteLength());

    return dup;
}


struct OSample
{
    SampleInputStream* input;
    Sample* samp;

    OSample(SampleInputStream* sis, Sample* s):
        input(sis), samp(s)
    {
    }
};

ostream& operator<<(ostream& os, const OSample& osamp)
{
    os << "[" << osamp.input->getName() << ": "
       << "tt=" << tformat(osamp.samp->getTimeTag())
       << " (" << osamp.samp->getDSMId()
       << "," << osamp.samp->getSpSId() << ")]";
    return os;
}


void
NidsMerge::printStats(dsm_time_t tcur, bool totals)
{
    static vector<header_t> window_headers = {
        {"readahead window start", 30},
        {"read", 7}, {"unique", 7}, {"nonincr", 7}, {"ni_free", 7},
        {"file", 0}
    };
    static vector<header_t> total_headers = {
        {"totals up to last time", 30},
        {"read", 7}, {"unique", 7}, {"nonincr", 7}, {"ni_free", 7},
        {"file", 0}
    };
    vector<header_t>& headers = totals ? total_headers : window_headers;
    print_headers(cout, headers);
    for (auto& input : inputs)
    {
        InputStats& stats = totals ? input.total_stats : input.window_stats;
        print_column(headers[0], tformat(tcur));
        print_column(headers[1], stats.nread);
        print_column(headers[2], stats.nunique);
        print_column(headers[3], stats.nnonincr);
        print_column(headers[4], stats.nnonincr_free);
        print_column(headers[5], input.stream->getName());
        cout << endl;
    }
}


int NidsMerge::run()
{
    nidas::core::FileSet* outSet = FileSet::createFileSet(outputFileName);
    outSet->setFileLengthSecs(outputFileLength);

    SampleOutputStream outStream(outSet);
    outStream.setHeaderSource(this);

    for (unsigned int ii = 0; ii < inputFileNames.size(); ii++)
    {
        addInputStream(inputFileNames[ii]);
    }

    SampleMatcher& matcher = _app.sampleMatcher();

    // samples with non-increasing time tags
    SortedSampleSet3 nonincr;
    bool forceIncreasingTimes = ForceIncreasingTimes.asBool();

    // Keep reading as long as there is at least one input stream that has not
    // reached EOF and until we've read at least one whole readahead period
    // past the end.  We want to avoid quitting exactly when tcur == endTime,
    // because then the loop hasn't had a chance to read past endTime to catch
    // any more samples that might also precede endTime.  The flushSorter()
    // method makes sure to never write samples outside the time range.
    dsm_time_t tcur;
    static nidas::util::LogContext duplog(LOG_VERBOSE);
    int nloops = 0;
    for (tcur = startTime.toUsecs();
         numOpen() > 0 &&
         (!endTime.isSet() || (tcur < endTime.toUsecs() + readAheadUsecs)) &&
         !_app.interrupted();
         tcur += readAheadUsecs)
    {
        nloops += 1;
        DLOG(("") << numOpen() << " inputs open, "
                  << "merge loop #" << nloops
                  << " at step: " << tformat(tcur));
        // if the start time has not been sent, then we do not know yet when
        // the first readahead window will end, so fill the readahead window
        // for each input with at least readAheadUsecs of data.  it doesn't
        // matter if the inputs start at different times, since after the
        // start time is determined, only the samples within that readahead
        // window will be output, the rest will stay in the sorter.
        dsm_time_t filltime =
            startTime.isMin() ? LONG_LONG_MAX : tcur + readAheadUsecs;
        for (unsigned int ii = 0;
             ii < inputs.size() && !_app.interrupted();
             ii++)
        {
            if (inputs[ii].eof)
                continue;
            SampleInputStream* input = inputs[ii].stream;
            dsm_time_t& lastTime = inputs[ii].lastTime;
            InputStats& stats = inputs[ii].window_stats;
            stats = InputStats(); // reset window stats

            DLOG(("") << "filling input " << input->getName()
                      << " up to filltime " << tformat(filltime)
                      << ", lastTime=" << tformat(lastTime));
            while (lastTime < filltime && !_app.interrupted())
            {
                Sample* samp{ nullptr };
                try {
                    samp = input->readSample();
                }
                catch (const EOFException& e) {
                    cerr << e.what() << endl;
                    lastTime = LONG_LONG_MAX;
                    inputs[ii].eof = true;
                    break;
                }

                // maybe we should "bind" a matcher instance to each
                // stream, so the filename matching does not need to be
                // done each time since it shouldn't change within the
                // same input stream...
                if (!matcher.match(samp, input->getName()))
                {
                    samp->freeReference();
                    continue;
                }

                lastTime = samp->getTimeTag();
                if (startTime.isMin() && lastTime + readAheadUsecs < filltime)
                {
                    // until start time is known, the fill window ends at the
                    // earliest time tag plus the read-ahead time.
                    DLOG(("adjusting fill time: ") <<
                         tformat(filltime) << " -> "
                         << tformat(lastTime + readAheadUsecs));
                    filltime = lastTime + readAheadUsecs;
                }
                // until startTime has been set, no samples can be
                // dropped.
                if (lastTime < startTime.toUsecs())
                {
                    ndropped += 1;
                    DLOG(("dropping sample ") << ndropped
                            << " precedes start "
                            << tformat(startTime.toUsecs()) << ": "
                            << OSample(input, samp));
                    samp->freeReference();
                    continue;
                }
                stats.nread++;

                // check if this time is not increasing WRT last sample from
                // this input with this sample id.  if not, then replace it
                // with a copy with an adjusted time tag, but keep the
                // original sample in the nonincr set, and check that set for
                // duplicates of the original sample.
                bool time_shifted = false;
                dsm_time_t lastSampleTime = LONG_LONG_MIN;

                if (getLastSampleTime(input, samp->getRawId(), lastSampleTime)
                    && lastTime <= lastSampleTime)
                {
                    stats.nnonincr++;
                    VLOG(("non-increasing sample: ") << OSample(input, samp)
                         << ", nnonincr=" << stats.nnonincr);

                    if (forceIncreasingTimes)
                    {
                        // sample with a non-increasing time. Save it, don't
                        // free its reference
                        nonincr.insert(samp);

                        // make a copy of the sample with 1 microsecond added
                        // to the time tag
                        Sample* corsamp = duplicateSample(samp);
                        corsamp->setTimeTag(lastSampleTime + 1);
                        lastTime = corsamp->getTimeTag();
                        samp = corsamp;
                        time_shifted = true;
                    }
                    // sample will be inserted into output sorter below
                }
                else if (forceIncreasingTimes)
                {
                    // only need to check for duplicates in nonincr set if
                    // time correction is being done.
                    SortedSampleSet3::const_iterator i3 = nonincr.find(samp);
                    if (i3 != nonincr.end()) {
                        assert((*i3)->getTimeTag() == samp->getTimeTag());
                        if (duplog.active() && (ii < inputs.size() - 1))
                        {
                            duplog.log()
                                << "found dup sample in non-increasing set, discarding: "
                                << OSample(input, samp)
                                << ", nnonincr=" << nonincr.size();
                        }
                        samp->freeReference();
                        stats.nnonincr_free++;
                        // done with this sample, continue without inserting
                        // it into output sorter.
                        continue;
                    }
                }

                if (lastTime > lastSampleTime)
                {
                    // update last sample time if it is newer
                    setLastSampleTime(input, samp->getRawId(), lastTime);
                }
                if (!sorter.insert(samp).second)
                {
                    // duplicate of sample already in the sorter set.
                    samp->freeReference();
                    if (duplog.active() && (ii < inputs.size() - 1))
                    {
                        duplog.log()
                            << (time_shifted ? "time-shifted " : "")
                            << "duplicate sample: " << OSample(input, samp);
                    }
                }
                else
                    stats.nunique++;
            }
        }
        // set startTime to the first time read across all inputs if user
        // did not specify it in the runstring.
        if (startTime.isMin() && filltime != LONG_LONG_MAX)
        {
            tcur = filltime - readAheadUsecs;
            startTime = tcur;
            DLOG(("set start time: ") << tformat(tcur));
        }

        // report stats for this readahead window
        printStats(tcur);
        for (auto& input : inputs)
        {
            input.total_stats += input.window_stats;
        }

        // all the inputs have been read up to the first sample past the
        // read-ahead time, so flush only up until the beginning of the
        // read-ahead period (tcur).  the samples in the individual input
        // streams are likely not in time order, so there will be more samples
        // which fall into the current read-ahead period.  if this is the
        // first time through, then only the first read-ahead period has been
        // read and no samples would be flushed yet, so don't bother calling
        // flushSorter().
        if (!_app.interrupted() && nloops != 1)
        {
            (void)flushSorter(tcur, outStream);
            // avoid log messages about aging off an empty nonincr set
            if (nonincr.size() > 0)
                ageOffNonIncr(tcur, nonincr);
        }
    }
    // All the samples have been read which need to be read, so flush all the
    // remaining samples.  Make it explicit by passing UTime::MAX as the
    // cutoff time.  If an end time was set, then flushSorter() will clip
    // samples at the end time.
    if (!_app.interrupted())
    {
        dsm_time_t last_written = flushSorter(UTime::MAX.toUsecs(), outStream);
        printStats(last_written, true);
    }
    outStream.flush();
    outStream.close();
    clearNonIncr(nonincr);
    size_t total_nonincr = 0;
    for (auto& input : inputs) {
        input.stream->close();
        delete input.stream;
        input.stream = nullptr;
        total_nonincr += input.total_stats.nnonincr;
    }
    if (total_nonincr > 0) {
        cerr << "Warning: Found " << total_nonincr
             << " samples with non-increasing times!" << endl;
        if (forceIncreasingTimes)
            cerr << "Sample times were adjusted to force increasing times."
                 << endl;
        else
            cerr << "Since --force-increasing-times was not enabled, "
                 << "some samples may have been sorted into a different "
                 << "order than they were read from the sensor." << endl;
    }

    cout << "Excluded " << matcher.numSamplesExcluded() << " samples "
         << "(out of " << matcher.numSamplesChecked() << ") "
         << "due to filter matching." << endl;
    cout << "Discarded " << ndropped << " samples whose times "
         << "were earlier than the merge window when read." << endl;

    return 0;
}
