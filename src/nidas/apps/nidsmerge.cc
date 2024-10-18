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

class NidsMerge: public HeaderSource
{
public:

    NidsMerge();

    int parseRunstring(int argc, char** argv);

    int run();

    int main(int argc, char** argv);

    int usage(const char* argv0);

    void sendHeader(dsm_time_t thead,SampleOutput* out);

private:

    void addInputStream(vector<SampleInputStream*>& inputs,
                        const list<string>& inputFiles);

    void flushSorter(dsm_time_t tcur, SampleOutputStream& outStream);

    vector<list<string> > inputFileNames;

    string outputFileName;

    vector<dsm_time_t> lastTimes;
    unsigned int neof;

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
    vector<size_t> samplesRead;
    vector<size_t> samplesUnique;

    unsigned long ndropped;

    NidasApp _app;

    BadSampleFilterArg FilterArg;
    NidasAppArg KeepOpening;
    NidasAppArg PrintHeader;
};

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
    inputFileNames(),outputFileName(),lastTimes(),neof(0),
    readAheadUsecs(30*USECS_PER_SEC),startTime(UTime::MIN),
    endTime(UTime::MAX), outputFileLength(0),header(),
    configName(), allowed_dsms(),
    sorter(),
    samplesRead(),
    samplesUnique(),
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
    NidasAppArg OutputFileLength
        ("-l,--length", "seconds",
         "Set length of output files in seconds.  This option is deprecated\n"
         "since it conflicts with the standard -l option for logging.\n"
         "Instead, use the @<seconds> output file name suffix to specify\n"
         "the output file length. The @ specifier takes precedence.\n"
         "Output file length is required if the output filename contains\n"
         "time specifiers.");

    app.enableArguments(app.LogConfig | app.LogShow | app.LogFields |
                        app.LogParam | app.StartTime | app.EndTime |
                        app.OutputFiles | app.Clipping | KeepOpening |
                        FilterArg | InputFileSet | InputFileSetFile |
                        ReadAhead | ConfigName | OutputFileLength |
                        app.SampleRanges | PrintHeader |
                        app.Version | app.Help);
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
        if (arg == &OutputFileLength)
            outputFileLength = OutputFileLength.asInt();
        else if (arg == &app.OutputFiles)
        {
            // Use the length suffix if given with the output,
            // otherwise revert to the last length option.
            if (app.outputFileLength() == 0 && OutputFileLength.specified())
                outputFileLength = OutputFileLength.asInt();
            else
                outputFileLength = app.outputFileLength();
        }
        else if (arg == &InputFileSet)
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
    outputFileName = app.outputFileName();
    if (outputFileName.length() == 0)
    {
        xmsg << "Output file name is required.";
        throw NidasAppException(xmsg.str());
    }
    if (outputFileLength == 0 &&
        outputFileName.find('%') != string::npos)
    {
        xmsg << "Output file length is required for "
                "output filenames with time specifiers.";
        throw NidasAppException(xmsg.str());
    }
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
    return UTime(dt).format(true, "%Y-%b-%d_%H:%M:%S.%f");
}


void
NidsMerge::flushSorter(dsm_time_t tcur,
                       SampleOutputStream& outStream)
{
    SampleT<char> dummy;
    SortedSampleSet3::const_iterator rsb = sorter.begin();

    // get iterator pointing at first sample equal to or greater
    // than dummy sample
    dummy.setTimeTag(tcur);
    SortedSampleSet3::const_iterator rsi = sorter.lower_bound(&dummy);

    for (SortedSampleSet3::const_iterator si = rsb; si != rsi; ++si)
    {
        const Sample *sample = *si;
        bool ok{ true };
        if (!endTime.isSet() || (sample->getTimeTag() < endTime.toUsecs()))
            ok = outStream.receive(sample);
        sample->freeReference();
        if (!ok)
            throw IOException("send sample",
                "Send failed, output disconnected.");
    }

    // remove samples from sorted set
    size_t before = sorter.size();
    if (rsi != rsb) sorter.erase(rsb, rsi);
    size_t after = sorter.size();

    cout << tformat(tcur);
    for (unsigned int ii = 0; ii < samplesRead.size(); ii++) {
        cout << ' ' << setw(7) << samplesRead[ii];
        cout << ' ' << setw(7) << samplesUnique[ii];
    }
    cout << setw(8) << before << ' ' << setw(7) << after << ' ' <<
        setw(7) << before - after << endl;
}


void
NidsMerge::addInputStream(vector<SampleInputStream*>& inputs,
                          const list<string>& inputFiles)
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

    DLOG(("getName=") << fset->getName());
    DLOG(("start time=") << startTime.format(true, "%c"));
    DLOG(("end time=") << endTime.format(true, "%c"));

    // SampleInputStream owns the iochan ptr.
    SampleInputStream* input = new SampleInputStream(fset);
    inputs.push_back(input);

    // Set the input stream filter in case other options were set
    // from the command-line that do not filter samples, like
    // skipping nidas input headers.
    BadSampleFilter& bsf = FilterArg.getFilter();
    bsf.setDefaultTimeRange(startTime, endTime);
    input->setBadSampleFilter(bsf);

    lastTimes.push_back(LONG_LONG_MIN);

    try {
        input->readInputHeader();
        // save header for later writing to output
        header = input->getInputHeader();
    }
    catch (const EOFException& e) {
        cerr << e.what() << endl;
        lastTimes.back() = LONG_LONG_MAX;
        neof++;
    }
    // IOException propagates to here only on an io read error or if
    // keep-opening is false and a file cannot be opened.  either are good
    // reasons to exit with an error, so let the exception propagate.
}


int NidsMerge::run()
{
    nidas::core::FileSet* outSet = FileSet::createFileSet(outputFileName);
    outSet->setFileLengthSecs(outputFileLength);

    SampleOutputStream outStream(outSet);
    outStream.setHeaderSource(this);

    vector<SampleInputStream*> inputs;
    for (unsigned int ii = 0; ii < inputFileNames.size(); ii++)
    {
        addInputStream(inputs, inputFileNames[ii]);
    }

    samplesRead = vector<size_t>(inputs.size(), 0);
    samplesUnique = vector<size_t>(inputs.size(), 0);
    SampleMatcher& matcher = _app.sampleMatcher();

    cout << "     date(GMT)      ";
    for (unsigned int ii = 0; ii < inputs.size(); ii++) {
        cout << "  input" << ii;
        cout << " unique" << ii;
    }
    cout << "    before   after  output" << endl;

    // Keep reading as long as there is at least one input stream that has not
    // reached EOF and until we've read at least one whole readahead period
    // past the end.  We want to avoid quitting exactly when tcur == endTime,
    // because then the loop hasn't had a chance to read past endTime to catch
    // any more samples that might also precede endTime.  The flushSorter()
    // method makes sure to never write samples outside the time range.
    dsm_time_t tcur;
    for (tcur = startTime.toUsecs();
         neof < inputs.size() &&
         (!endTime.isSet() || (tcur < endTime.toUsecs() + readAheadUsecs)) &&
         !_app.interrupted();
         tcur += readAheadUsecs)
    {
        DLOG(("merge loop at step: ") << tformat(tcur));
        for (unsigned int ii = 0;
             ii < inputs.size() && !_app.interrupted();
             ii++)
        {
            SampleInputStream* input = inputs[ii];
            dsm_time_t lastTime = lastTimes[ii];
            size_t nread = 0;
            size_t nunique = 0;

            while (lastTime < tcur + readAheadUsecs && !_app.interrupted())
            {
                Sample* samp{ nullptr };
                try {
                    samp = input->readSample();
                }
                catch (const EOFException& e) {
                    cerr << e.what() << endl;
                    lastTime = LONG_LONG_MAX;
                    ++neof;
                    break;
                }
                lastTime = samp->getTimeTag();

                // maybe we should "bind" a matcher instance to each
                // stream, so the filename matching does not need to be
                // done each time since it shouldn't change within the
                // same input stream...
                if (!matcher.match(samp, input->getName()))
                {
                    samp->freeReference();
                    continue;
                }

                // until startTime has been set, no samples can be
                // dropped.
                if (lastTime < startTime.toUsecs())
                {
                    ndropped += 1;
                    DLOG(("dropping sample ") << ndropped
                            << " precedes start "
                            << tformat(startTime.toUsecs()) << ": "
                            << "(" << samp->getDSMId() << ","
                            << samp->getSpSId() << ")"
                            << " at " << tformat(lastTime));
                    samp->freeReference();
                }
                else if (!sorter.insert(samp).second)
                {
                    // duplicate of sample already in the sorter set.
                    samp->freeReference();
                }
                else
                    nunique++;
                nread++;
            }
            lastTimes[ii] = lastTime;

            // set startTime to the first time read across all inputs if user
            // did not specify it in the runstring.  conveniently, the
            // earliest time is the first in the sorter.
            if (startTime.isMin() && sorter.size() > 0)
            {
                startTime = (*sorter.begin())->getTimeTag();
                tcur = startTime.toUsecs();
            }

            samplesRead[ii] = nread;
            samplesUnique[ii] = nunique;
        }
        // all the inputs have been read up to the first sample past the
        // read-ahead time, so flush only up until the beginning of the
        // read-ahead period (tcur).  the samples in the individual input
        // streams are likely not in time order, so there will be more samples
        // which fall into the current read-ahead period.
        if (!_app.interrupted())
        {
            flushSorter(tcur, outStream);
        }
    }
    // All the samples have been read which need to be read, so flush all the
    // remaining samples.  Make it explicit by passing UTime::MAX as the
    // cutoff time.  If an end time was set, then flushSorter() will clip
    // samples at the end time.
    if (!_app.interrupted())
    {
        flushSorter(UTime::MAX.toUsecs(), outStream);
    }
    outStream.flush();
    outStream.close();
    for (unsigned int ii = 0; ii < inputs.size(); ii++) {
        SampleInputStream* input = inputs[ii];
        input->close();
        delete input;
    }

    cout << "Excluded " << matcher.numSamplesExcluded() << " samples "
         << "(out of " << matcher.numSamplesChecked() << ") "
         << "due to filter matching." << endl;
    cout << "Discarded " << ndropped << " samples whose times "
         << "were earlier than the merge window when read." << endl;

    return 0;
}
