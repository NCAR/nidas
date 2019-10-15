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

#include <unistd.h>
#include <getopt.h>

#include <csignal>
#include <climits>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

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

    bool receiveAllowedDsm(SampleOutputStream &, const Sample *); //Write sample if allowed


    vector<list<string> > inputFileNames;

    string outputFileName;

    vector<dsm_time_t> lastTimes;

    long long readAheadUsecs;

    n_u::UTime startTime;
 
    n_u::UTime endTime;

    int outputFileLength;

    SampleInputHeader header;

    string configName;

    bool _filterTimes;

    list<unsigned int> allowed_dsms; /* DSMs to require.  If empty*/

    NidasApp _app;
};

int main(int argc, char** argv)
{
    return NidsMerge::main(argc,argv);
}

/* static */
int NidsMerge::usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " [-x config] -i input ...  [-i input ... ] ..." << endl;
    cerr << "    [-s start_time] [-e end_time]" << endl;
    cerr << endl;
    cerr << "    -o output [-l output_file_length] [-r read_ahead_secs]" << endl;
    cerr << "    -c config: Legacy flag for -x." << endl;
    cerr << "    -x config: Update the configuration name in the output header. Legacy -c" << endl;
    cerr << "         example: -x $ISFF/projects/AHATS/ISFF/config/ahats.xml" << endl;
    cerr << "    -f : filter sample timetags. If a sample timetag does not fall" << endl;
    cerr << "         between start and end time, assume sample header is corrupt" << endl;
    cerr << "         and scan ahead for a good header. Use only on corrupt data files." << endl;
    cerr << "    -i input ...: one or more input file name or file name formats" << endl;
    cerr << "    -d dsm ...: one or more DSM IDs to require input data tagged with. If this"  << endl;
    cerr << "         option is ommited (default), then any input data will be passed"  << endl;
    cerr << "         blindly to the output.  If any dsms are defined here, only input"  << endl;
    cerr << "         samples with same DSM IDs given here will be passed to the output." << endl;
    cerr << "    -s start_time" << endl;
    cerr << "    -e end_time: time period to merge" << endl;
    cerr << "    -o output: output file name or file name format" << endl;
    cerr << "    -l output_file_length: length of output files, in seconds" << endl;
    cerr << "    -r read_ahead_secs: how much time to read ahead and sort the input samples" << endl;
    cerr << "         before outputting the sorted, merged samples" << endl;
    cerr << "\nStandard nidas options:" << endl;
    cerr << _app.usage() << endl;
    cerr << endl;
    cerr << "Example (from ISFF/TREX): \n" << argv0 << endl;
    cerr << "   -i /data1/isff_%Y%m%d_%H%M%S.dat " << endl;
    cerr << "    -i /data2/central_%Y%m%d_%H%M%S.dat" << endl;
    cerr << "    -i /data2/south_%Y%m%d_%H%M%S.dat" << endl;
    cerr << "    -i /data2/west_%Y%m%d_%H%M%S.dat" << endl;
    cerr << "    -o /data3/isff_%Y%m%d_%H%M%S.dat -l 14400 -r 10" << endl;
    cerr << "    -s \"2006 Apr 1 00:00\" -e \"2006 Apr 10 00:00\"" << endl;
    return 1;
}

/* static */
int NidsMerge::main(int argc, char** argv) throw()
{
    NidasApp::setupSignals();

    NidsMerge merge;

    int res;
    
    if ((res = merge.parseRunstring(argc,argv)) != 0) return res;

    return merge.run();
}


NidsMerge::NidsMerge():
    inputFileNames(),outputFileName(),lastTimes(),
    readAheadUsecs(30*USECS_PER_SEC),startTime(LONG_LONG_MIN),
    endTime(LONG_LONG_MAX), outputFileLength(0),header(),
    configName(),_filterTimes(false), allowed_dsms(),
    _app("nidsmerge")
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
    app.enableArguments(app.LogConfig | app.LogShow | app.LogFields |
                        app.LogParam | app.StartTime | app.EndTime |
                        app.Version | app.OutputFiles |
                        app.Help);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = false;
    // -l conflicts with output file length.
    app.LogConfig.acceptShortFlag(false);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        usage(argv[0]);
    }

    NidasAppArgv left(argv[0], args);
    int opt_char;     /* option character */

    while ((opt_char = getopt(left.argc, left.argv, "-c:x:fil:r:d:")) != -1) {
    switch (opt_char) {
    case 'x':
    case 'c':
        configName = optarg;
        break;
    case 'f':
        _filterTimes = true;
        break;
    case 'i':
        {
        list<string> fileNames;
        while(optind < argc && argv[optind][0] != '-') {
            fileNames.push_back(argv[optind++]);
        }
        inputFileNames.push_back(fileNames);
        }
        break;
    case 'l':
        outputFileLength = atoi(optarg);
        break;
    case 'r':
        readAheadUsecs = atoi(optarg) * (long long)USECS_PER_SEC;
        break;
    case 'd':
        cout << "Allowing Data with ID=" << optarg << endl;
        allowed_dsms.push_back(atoi(optarg));
        break;
    case '?':
        return usage(argv[0]);
    }
    }
    outputFileName = app.outputFileName();
    if (outputFileLength == 0)
    {
        outputFileLength = app.outputFileLength();
    }
    startTime = app.getStartTime();
    endTime = app.getEndTime();
    if (inputFileNames.size() == 0) return usage(argv[0]);
    for (unsigned int ii = 0; ii < inputFileNames.size(); ii++) {
        const list<string>& inputFiles = inputFileNames[ii];
        list<string>::const_iterator fi = inputFiles.begin();
        if (inputFiles.size() == 1 && fi->find('%') != string::npos &&
            (startTime.toUsecs() == LONG_LONG_MIN ||
             endTime.toUsecs() == LONG_LONG_MAX)) {
            cerr << "ERROR: start and end times not set, and file name has a % descriptor" << endl;
            return usage(argv[0]);
        }
    }

    //setup DSM IDs if provided

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
    receiveAllowedDsm writes the passed sample to the passed stream if the DSM id of the sample
    is in allowed_dsms.  If allowed_dsms is empty, the sample is written to the stream.  Returns
    whatever stream.receive(sample) returns if the sample's DSM is in the correct range, otherwise
    false, 
*/
bool NidsMerge::receiveAllowedDsm(SampleOutputStream &stream, const Sample * sample)
{
    if (allowed_dsms.size() == 0)
    {
        return stream.receive(sample);
    }
    unsigned int want = sample->getDSMId();
    for (list<unsigned int>::const_iterator i = allowed_dsms.begin(); i !=  allowed_dsms.end(); i++)
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

#ifdef DEBUG
            cerr << "getName=" << fset->getName() << endl;
            cerr << "start time=" << startTime.format(true,"%c") << endl;
            cerr << "end time=" << endTime.format(true,"%c") << endl;
#endif

            // SampleInputStream owns the iochan ptr.
            SampleInputStream* input = new SampleInputStream(fset);
            inputs.push_back(input);
            input->setMaxSampleLength(32768);

            if (_filterTimes) {
                n_u::UTime filter1(startTime - USECS_PER_DAY);
                n_u::UTime filter2(endTime + USECS_PER_DAY);
                input->setMinSampleTime(filter1);
                input->setMaxSampleTime(filter2);
            }

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

#ifdef ADDITIONAL_TIME_FILTERS
                /* this won't really work, since the next sample from input
                 * may legitimately be a day or more ahead as the result
                 * of a typical data gap, or late start of a system.
                 */
                n_u::UTime filter1(tcur - USECS_PER_HOUR * 3);
                n_u::UTime filter2(tcur + readAheadUsecs + USECS_PER_HOUR * 3);
                input->setMinSampleTime(filter1);
                input->setMaxSampleTime(filter2);
#endif

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

            cout << n_u::UTime(tcur).format(true,"%Y %b %d %H:%M:%S");
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

            cout << n_u::UTime(tcur).format(true,"%Y %b %d %H:%M:%S");
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
