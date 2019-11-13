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

namespace n_u = nidas::util;

using nidas::util::UTime;

class NidsMerge: public HeaderSource
{
public:

    NidsMerge();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

    static int main(int argc, char** argv) throw();

    int usage(const char* argv0);

    void sendHeader(dsm_time_t thead,SampleOutput* out)
        throw(n_u::IOException);
    
    /**
     * for debugging.
     */
    void printHeader();

private:

    // Write sample if allowed
    bool receiveAllowedDsm(SampleOutputStream &, const Sample *);

    vector<list<string> > inputFileNames;

    string outputFileName;

    vector<dsm_time_t> lastTimes;

    long long readAheadUsecs;

    UTime startTime;
 
    UTime endTime;

    int outputFileLength;

    SampleInputHeader header;

    string configName;

    list<unsigned int> allowed_dsms; /* DSMs to require.  If empty*/

    NidasApp _app;

    BadSampleFilterArg FilterArg;
    NidasAppArg KeepOpening;
};

int main(int argc, char** argv)
{
    return NidsMerge::main(argc,argv);
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

/* static */
int NidsMerge::main(int argc, char** argv) throw()
{
    NidasApp::setupSignals();

    NidsMerge merge;

    int res = merge.parseRunstring(argc, argv);

    if (res != 0)
        return res;
    return merge.run();
}


NidsMerge::NidsMerge():
    inputFileNames(),outputFileName(),lastTimes(),
    readAheadUsecs(30*USECS_PER_SEC),startTime(LONG_LONG_MIN),
    endTime(LONG_LONG_MAX), outputFileLength(0),header(),
    configName(), allowed_dsms(),
    _app("nidsmerge"),
    FilterArg(),
    KeepOpening
    ("--keep-opening", "",
     "Open the next file when an error occurs instead of stopping.",
     "true")
{
}

int NidsMerge::parseRunstring(int argc, char** argv) throw()
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
         "Output file length is required, there is no default.");
    NidasAppArg DSMid
        ("-d,--dsm", "id",
         "DSM id to accept from the input data. By default,\n"
         "all input samples are passed to the merge.  If any DSM IDs are\n"
         "specified with -d, then only input samples from those DSMs\n"
         "will be included in the merge. Multiple -d options are allowed.\n");

    app.enableArguments(app.LogConfig | app.LogShow | app.LogFields |
                        app.LogParam | app.StartTime | app.EndTime |
                        app.Version | app.OutputFiles | KeepOpening |
                        FilterArg | InputFileSet | ReadAhead |
                        ConfigName | OutputFileLength | DSMid |
                        app.Help);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = false;
    // -l conflicts with output file length.
    app.LogConfig.acceptShortFlag(false);

    bool requiretimes = false;
    try {
        app.startArgs(argc, argv);
        std::ostringstream xmsg;
        NidasAppArg* arg;
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
            else if (arg == &DSMid)
                allowed_dsms.push_back(DSMid.asInt());
            else if (arg == &InputFileSet)
            {
                // First argument has already been retrieved.
                list<string> fileNames;
                string filespec = InputFileSet.getValue();
                bool timespecs = false;
                // Collect any additional filenames in the file set up
                // until the next option specified.
                do {
                    if (filespec.find('%') != string::npos)
                        timespecs = true;
                    fileNames.push_back(filespec);
                } while (app.nextArg(filespec));

                if (timespecs && fileNames.size() != 1)
                {
                    xmsg << "Only one filespec allowed in a file set "
                         << "with time specifiers : " << *fileNames.begin()
                         << " ... " << *fileNames.rbegin();
                    throw NidasAppException(xmsg.str());
                }
                requiretimes |= timespecs;
                inputFileNames.push_back(fileNames);
            }
        }
        readAheadUsecs = ReadAhead.asInt() * (long long)USECS_PER_SEC;
        configName = ConfigName.getValue();
        if (app.helpRequested())
        {
            return usage(argv[0]);
        }
        startTime = app.getStartTime();
        endTime = app.getEndTime();
        outputFileName = app.outputFileName();
        if (outputFileName.length() == 0)
        {
            xmsg << "Output file name is required.";
            throw NidasAppException(xmsg.str());
        }
        if (outputFileLength == 0)
        {
            xmsg << "Output file length is required.";
            throw NidasAppException(xmsg.str());
        }
        if (requiretimes && (startTime.toUsecs() == LONG_LONG_MIN ||
                             endTime.toUsecs() == LONG_LONG_MAX))
        {
            xmsg << "Start and end times must be set when a fileset uses "
                 << "a % time specifier.";
            throw NidasAppException(xmsg.str());
        }
    }
    catch (NidasAppException& ex)
    {
        std::cerr << ex.what() << std::endl;
        std::cerr << "Use -h to see usage info." << std::endl;
        return 1;
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
    throw(n_u::IOException)
{
    if (configName.length() > 0)
        header.setConfigName(configName);
    printHeader();
    header.write(out);
}

void NidsMerge::printHeader()
{
    cerr << "ArchiveVersion:" << header.getArchiveVersion() << endl;
    cerr << "SoftwareVersion:" << header.getSoftwareVersion() << endl;
    cerr << "ProjectName:" << header.getProjectName() << endl;
    cerr << "SystemName:" << header.getSystemName() << endl;
    cerr << "ConfigName:" << header.getConfigName() << endl;
    cerr << "ConfigVersion:" << header.getConfigVersion() << endl;
}

/**
    receiveAllowedDsm writes the passed sample to the passed stream if the
    DSM id of the sample is in allowed_dsms.  If allowed_dsms is empty, the
    sample is written to the stream.  Returns whatever
    stream.receive(sample) returns if the sample's DSM is in the correct
    range, otherwise false,
*/
bool NidsMerge::receiveAllowedDsm(SampleOutputStream &stream, const Sample * sample)
{
    if (allowed_dsms.size() == 0)
    {
        return stream.receive(sample);
    }
    unsigned int want = sample->getDSMId();
    for (list<unsigned int>::const_iterator i = allowed_dsms.begin();
         i !=  allowed_dsms.end(); i++)
    {
        if (*i == want) 
            return stream.receive(sample);
    }
    return false;
}

int NidsMerge::run() throw()
{
    try {
        nidas::core::FileSet* outSet = 0;
#ifdef HAVE_BZLIB_H
        if (outputFileName.find(".bz2") != string::npos)
            outSet = new nidas::core::Bzip2FileSet();
        else
#endif
        {
            outSet = new nidas::core::FileSet();
        }
        outSet->setFileName(outputFileName);
        outSet->setFileLengthSecs(outputFileLength);

        SampleOutputStream outStream(outSet);
        outStream.setHeaderSource(this);

        vector<SampleInputStream*> inputs;

        for (unsigned int ii = 0; ii < inputFileNames.size(); ii++) {

            const list<string>& inputFiles = inputFileNames[ii];

            nidas::core::FileSet* fset;

            list<string>::const_iterator fi = inputFiles.begin();
            if (inputFiles.size() == 1 && fi->find('%') != string::npos)
            {
#ifdef HAVE_BZLIB_H
                if (fi->find(".bz2") != string::npos)
                    fset = new nidas::core::Bzip2FileSet();
                else
#endif
                    fset = new nidas::core::FileSet();
                fset->setFileName(*fi);
                fset->setStartTime(startTime);
                fset->setEndTime(endTime);
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

            // input->init();

            try {
                input->readInputHeader();
                // save header for later writing to output
                header = input->getInputHeader();
            }
            catch (const n_u::EOFException& e) {
                cerr << e.what() << endl;
                lastTimes[ii] = LONG_LONG_MAX;
            }
            catch (const n_u::IOException& e) {
                if (e.getErrno() != ENOENT) throw e;
                cerr << e.what() << endl;
                lastTimes[ii] = LONG_LONG_MAX;
            }
        }

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
        SampleT<char> dummy;
        vector<size_t> samplesRead(inputs.size(),0);
        vector<size_t> samplesUnique(inputs.size(),0);

        cout << "     date(GMT)      ";
        for (unsigned int ii = 0; ii < inputs.size(); ii++) {
            cout << "  input" << ii;
            cout << " unique" << ii;
        }
        cout << "    before   after  output" << endl;

        unsigned int neof = 0;

        dsm_time_t tcur;
        for (tcur = startTime.toUsecs(); neof < inputs.size() && tcur < endTime.toUsecs();
             tcur += readAheadUsecs) {
            for (unsigned int ii = 0; ii < inputs.size(); ii++) {
                SampleInputStream* input = inputs[ii];
                size_t nread = 0;
                size_t nunique = 0;

                try {
                    dsm_time_t lastTime = lastTimes[ii];
                    while (!_app.interrupted() && lastTime < tcur + readAheadUsecs) {
                        Sample* samp = input->readSample();
                        lastTime = samp->getTimeTag();
                        // set startTime to the first time read if user
                        // did not specify it in the runstring.
                        if (startTime.toUsecs() == LONG_LONG_MIN) {
                            startTime = lastTime;
                            tcur = startTime.toUsecs();
                        }
                        if (lastTime < startTime.toUsecs() || !sorter.insert(samp).second)
                            samp->freeReference();
                        else nunique++;
                        nread++;
                    }
                    lastTimes[ii] = lastTime;
                }
                catch (const n_u::EOFException& e) {
                    cerr << e.what() << endl;
                    lastTimes[ii] = LONG_LONG_MAX;
                    neof++;
                }
                catch (const n_u::IOException& e) {
                    if (e.getErrno() != ENOENT) throw e;
                    cerr << e.what() << endl;
                    lastTimes[ii] = LONG_LONG_MAX;
                    neof++;
                }
                samplesRead[ii] = nread;
                samplesUnique[ii] = nunique;
                if (_app.interrupted()) break;
            }
            if (_app.interrupted()) break;

            SortedSampleSet3::const_iterator rsb = sorter.begin();

            // get iterator pointing at first sample equal to or greater
            // than dummy sample
            dummy.setTimeTag(tcur);
            SortedSampleSet3::const_iterator rsi = sorter.lower_bound(&dummy);

            for (SortedSampleSet3::const_iterator si = rsb; si != rsi; ++si) {
                const Sample *s = *si;
                if (s->getTimeTag() >= startTime.toUsecs())
                    receiveAllowedDsm(outStream, s);
                s->freeReference();
            }

            // remove samples from sorted set
            size_t before = sorter.size();
            if (rsi != rsb) sorter.erase(rsb,rsi);
            size_t after = sorter.size();

            cout << UTime(tcur).format(true,"%Y %b %d %H:%M:%S");
            for (unsigned int ii = 0; ii < inputs.size(); ii++) {
                cout << ' ' << setw(7) << samplesRead[ii];
                cout << ' ' << setw(7) << samplesUnique[ii];
            }
            cout << setw(8) << before << ' ' << setw(7) << after << ' ' <<
                setw(7) << before - after << endl;
        }
        if (!_app.interrupted()) {
            SortedSampleSet3::const_iterator rsb = sorter.begin();

            // get iterator pointing at first sample equal to or greater
            // than dummy sample
            dummy.setTimeTag(tcur);
            SortedSampleSet3::const_iterator rsi = sorter.lower_bound(&dummy);

            for (SortedSampleSet3::const_iterator si = rsb; si != rsi; ++si) {
                const Sample *s = *si;
                if (s->getTimeTag() >= startTime.toUsecs())
                    receiveAllowedDsm(outStream, s);
                s->freeReference();
            }

            // remove samples from sorted set
            size_t before = sorter.size();
            if (rsi != rsb) sorter.erase(rsb,rsi);
            size_t after = sorter.size();

            cout << UTime(tcur).format(true,"%Y %b %d %H:%M:%S");
            for (unsigned int ii = 0; ii < inputs.size(); ii++) {
                cout << ' ' << setw(7) << samplesRead[ii];
                cout << ' ' << setw(7) << samplesUnique[ii];
            }
            cout << setw(8) << before << ' ' << setw(7) << after << ' ' <<
                setw(7) << before - after << endl;
        }
        outStream.flush();
        outStream.close();
        for (unsigned int ii = 0; ii < inputs.size(); ii++) {
            SampleInputStream* input = inputs[ii];
            input->close();
            delete input;
        }
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
        return 1;
    }
    return 0;
}
