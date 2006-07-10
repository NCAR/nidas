/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-08-29 15:10:54 -0600 (Mon, 29 Aug 2005) $

    $LastChangedRevision: 2753 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/src/data_dump.cc $
 ********************************************************************

*/

#include <nidas/dynld/FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/SortedSampleSet.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>

#include <csignal>
#include <climits>

#include <iomanip>

#ifndef LLONG_MAX
#   define LLONG_MAX    9223372036854775807LL
#   define LLONG_MIN    (-LLONG_MAX - 1LL)
#endif

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class NidsMerge
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

private:

    static bool interrupted;

    vector<list<string> > inputFileNames;

    string outputFileName;

    vector<dsm_time_t> lastTimes;

    long readAheadUsecs;

    n_u::UTime startTime;
 
    n_u::UTime endTime;

    int outputFileLength;

};

int main(int argc, char** argv)
{
    return NidsMerge::main(argc,argv);
}


/* static */
bool NidsMerge::interrupted = false;

/* static */
void NidsMerge::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
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
Usage: " << argv0 << " -i input ...  [-i input ... ] ...\n\
	[-s start_time] [-e end_time]\n\
	-o output [-l output_file_length] [-r read_ahead_secs]\n\n\
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
	readAheadUsecs(30*USECS_PER_SEC),outputFileLength(0)
{
}

int NidsMerge::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "-e:il:o:s:r:")) != -1) {
	switch (opt_char) {
	case 'e':
	    try {
		endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch (const n_u::ParseException& pe) {
	        cerr << pe.what() << endl;
		return usage(argv[0]);
	    }
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
	    readAheadUsecs = atoi(optarg) * USECS_PER_SEC;
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
    return 0;
}

int NidsMerge::run() throw()
{

    try {
	FileSet* outSet = new nidas::dynld::FileSet();
	outSet->setFileName(outputFileName);
	outSet->setFileLengthSecs(outputFileLength);

	SampleOutputStream outStream(outSet);
	outStream.init();

	vector<SampleInputStream*> inputs;
	bool headerWritten = false;

	for (unsigned int ii = 0; ii < inputFileNames.size(); ii++) {

	    const list<string>& inputFiles = inputFileNames[ii];

	    FileSet* fset = new nidas::dynld::FileSet();
	    fset->setStartTime(startTime.toUsecs() / USECS_PER_SEC);
	    fset->setEndTime(endTime.toUsecs() / USECS_PER_SEC);

	    list<string>::const_iterator fi = inputFiles.begin();
	    for (; fi != inputFiles.end(); ++fi) {
		if (inputFiles.size() == 1 && 
			fi->find('%') != string::npos) fset->setFileName(*fi);
		else fset->addFileName(*fi);
	    }
#ifdef DEBUG
	    cerr << "getFileName=" << fset->getFileName() << endl;
	    cerr << "start time=" << startTime.format(true,"%c") << endl;
	    cerr << "end time=" << endTime.format(true,"%c") << endl;
#endif

	    // SampleInputStream owns the iochan ptr.
	    SampleInputStream* input = new SampleInputStream(fset);

	    input->init();

	    input->readHeader();
	    SampleInputHeader header = input->getHeader();
	    if (!headerWritten) {
		outStream.setHeader(header);
		headerWritten = true;
	    }
	    string systemName = header.getSystemName();

	    inputs.push_back(input);
	    lastTimes.push_back(LLONG_MIN);
	}

	SortedSampleSet2 sorter;
	SampleT<char> dummy;
	vector<size_t> samplesRead(inputs.size(),0);

	cout << "     date(GMT)      ";
	for (unsigned int ii = 0; ii < inputs.size(); ii++)
	    cout << "  input" << ii;
	cout << "   before   after  output" << endl;

	for (dsm_time_t tcur = startTime.toUsecs(); tcur <= endTime.toUsecs();
	    tcur += readAheadUsecs) {
	    for (unsigned int ii = 0; ii < inputs.size(); ii++) {
		SampleInputStream* input = inputs[ii];
		size_t nread = 0;
		try {
		    dsm_time_t lastTime = lastTimes[ii];
		    while (!interrupted && lastTime < tcur + readAheadUsecs) {
			Sample* samp = input->readSample();
			lastTime = samp->getTimeTag();
			if (!sorter.insert(samp).second) samp->freeReference();
			nread++;
		    }
		    lastTimes[ii] = lastTime;
		}
		catch (const n_u::EOFException& e) {
		    cerr << e.what() << endl;
		    lastTimes[ii] = LLONG_MAX;
		}
		samplesRead[ii] = nread;
		if (interrupted) break;
	    }
	    if (interrupted) break;

	    SortedSampleSet2::const_iterator rsb = sorter.begin();

	    // get iterator pointing at first sample equal to or greater
	    // than dummy sample
	    dummy.setTimeTag(tcur);
	    SortedSampleSet2::const_iterator rsi = sorter.lower_bound(&dummy);

	    for (SortedSampleSet2::const_iterator si = rsb; si != rsi; ++si) {
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
	    for (unsigned int ii = 0; ii < inputs.size(); ii++)
	    	cout << ' ' << setw(7) << samplesRead[ii];
	    cout << setw(7) << before << ' ' << setw(7) << after << ' ' <<
	    	setw(7) << before - after << endl;
	}
	outStream.finish();
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
