// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    void sendHeader(dsm_time_t thead,SampleOutput* out)
        throw(n_u::IOException);
    
    /**
     * for debugging.
     */
    void printHeader();

private:

    static bool interrupted;

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

};

int main(int argc, char** argv)
{
    return NidsMerge::main(argc,argv);
}


/* static */
bool NidsMerge::interrupted = false;

/* static */
void NidsMerge::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            NidsMerge::interrupted = true;
    break;
    }
}

/* static */
void NidsMerge::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                                                
    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = NidsMerge::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int NidsMerge::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-c config] -i input ...  [-i input ... ] ...\n\
	[-s start_time] [-e end_time]\n\
	-o output [-l output_file_length] [-r read_ahead_secs]\n\n\
    -c config: Update the configuration name in the output header\n\
        example: -c $ISFF/projects/AHATS/ISFF/config/ahats.xml\n\
    -f : filter sample timetags. If a sample timetag does not fall\n\
         between start and end time, assume sample header is corrupt\n\
         and scan ahead for a good header. Use only on corrupt data files.\n\
    -i input ...: one or more input file name or file name formats\n\
    -s start_time\n\
    -e end_time: time period to merge\n\
    -o output: output file name or file name format\n\
    -l output_file_length: length of output files, in seconds\n\
    -r read_ahead_secs: how much time to read ahead and sort the input samples\n\
    	before outputting the sorted, merged samples\n\n\
Example (from ISFF/TREX): \n" << argv0 << " \
-i /data1/isff_%Y%m%d_%H%M%S.dat \n\
	-i /data2/central_%Y%m%d_%H%M%S.dat\n\
	-i /data2/south_%Y%m%d_%H%M%S.dat\n\
	-i /data2/west_%Y%m%d_%H%M%S.dat\n\
	-o /data3/isff_%Y%m%d_%H%M%S.dat -l 14400 -r 10\n\
	-s \"2006 Apr 1 00:00\" -e \"2006 Apr 10 00:00\"\n\
" << endl;
    return 1;
}

/* static */
int NidsMerge::main(int argc, char** argv) throw()
{
    setupSignals();

    NidsMerge merge;

    int res;
    
    if ((res = merge.parseRunstring(argc,argv)) != 0) return res;

    return merge.run();
}


NidsMerge::NidsMerge():
    inputFileNames(),outputFileName(),lastTimes(),
    readAheadUsecs(30*USECS_PER_SEC),startTime(LONG_LONG_MIN),
    endTime(LONG_LONG_MAX), outputFileLength(0),header(),
    configName(),_filterTimes(false)
{
}

int NidsMerge::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "-c:e:fil:o:s:r:")) != -1) {
	switch (opt_char) {
	case 'c':
            configName = optarg;
	    break;
	case 'e':
	    try {
		endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
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
	case 'o':
	    outputFileName = optarg;
	    break;
	case 'r':
	    readAheadUsecs = atoi(optarg) * (long long)USECS_PER_SEC;
	    break;
	case 's':
	    try {
		startTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
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

int NidsMerge::run() throw()
{

    try {
	nidas::core::FileSet* outSet = 0;
#ifdef HAVE_BZLIB_H
        if (outputFileName.find(".bz2") != string::npos)
            outSet = new nidas::core::Bzip2FileSet();
        else
#endif
            outSet = new nidas::core::FileSet();
	outSet->setFileName(outputFileName);
	outSet->setFileLengthSecs(outputFileLength);

        SampleOutputStream outStream(outSet);
        outStream.setHeaderSource(this);

	vector<SampleInputStream*> inputs;

	for (unsigned int ii = 0; ii < inputFileNames.size(); ii++) {

	    const list<string>& inputFiles = inputFileNames[ii];

	    nidas::core::FileSet* fset;

	    list<string>::const_iterator fi = inputFiles.begin();
            if (inputFiles.size() == 1 && fi->find('%') != string::npos) {
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
            else fset = nidas::core::FileSet::getFileSet(inputFiles);

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
		    while (!interrupted && lastTime < tcur + readAheadUsecs) {
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
		if (interrupted) break;
	    }
	    if (interrupted) break;

	    SortedSampleSet3::const_iterator rsb = sorter.begin();

	    // get iterator pointing at first sample equal to or greater
	    // than dummy sample
	    dummy.setTimeTag(tcur);
	    SortedSampleSet3::const_iterator rsi = sorter.lower_bound(&dummy);

	    for (SortedSampleSet3::const_iterator si = rsb; si != rsi; ++si) {
		const Sample *s = *si;
		if (s->getTimeTag() >= startTime.toUsecs())
		    outStream.receive(s);
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
        if (!interrupted) {
	    SortedSampleSet3::const_iterator rsb = sorter.begin();

	    // get iterator pointing at first sample equal to or greater
	    // than dummy sample
	    dummy.setTimeTag(tcur);
	    SortedSampleSet3::const_iterator rsi = sorter.lower_bound(&dummy);

	    for (SortedSampleSet3::const_iterator si = rsb; si != rsi; ++si) {
		const Sample *s = *si;
		if (s->getTimeTag() >= startTime.toUsecs())
		    outStream.receive(s);
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
	    delete input;
	}
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}
