/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-02-26 14:53:40 -0700 (Mon, 26 Feb 2007) $

    $LastChangedRevision: 3684 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/nidsmerge.cc $
 ********************************************************************

*/

#include <nidas/core/FileSet.h>
#include <nidas/core/Bzip2FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/SortedSampleSet.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>

#include <csignal>
#include <climits>

#include <iomanip>

// hack for arm-linux-gcc from Arcom which doesn't define LLONG_MAX
#ifndef LLONG_MAX
#   define LLONG_MAX    9223372036854775807LL
#   define LLONG_MIN    (-LLONG_MAX - 1LL)
#endif

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class MergeVerifier
{
public:

    MergeVerifier();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    /**
     * for debugging.
     */
    void printHeader(const SampleInputHeader& header);

    void reportMissing(SampleInputStream* input,
        SampleInputStream* merge, Sample* samp);

    void reportBackward(int nback,SampleInputStream* input,Sample* samp);

    void reportDuplicate(unsigned ndup, SampleInputStream* merge, Sample* samp);

private:

    static bool interrupted;

    vector<list<string> > inputFileNames;

    list<string> mergeFileNames;

    long readAheadUsecs;

    n_u::UTime startTime;
 
    n_u::UTime endTime;

    size_t nmissing;

};

int main(int argc, char** argv)
{
    return MergeVerifier::main(argc,argv);
}


/* static */
bool MergeVerifier::interrupted = false;

/* static */
void MergeVerifier::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            MergeVerifier::interrupted = true;
    break;
    }
}

/* static */
void MergeVerifier::setupSignals()
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
    act.sa_sigaction = MergeVerifier::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int MergeVerifier::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " -i input ...  [-i input ... ] ...\n\
	[-s start_time] [-e end_time]\n\
	-o output [-r read_ahead_secs]\n\n\
    -i input ...: one or more input file name or file name formats\n\
    -s start_time\n\
    -e end_time: time period to merge\n\
    -m merge: merge file name or file name format\n\
    -r read_ahead_secs: how much time to read ahead and sort the input samples\n\
    	before verifying\n\n\
Example (from ISFF/TREX): \n" << argv0 << " \
-i /data1/isff_%Y%m%d_%H%M%S.dat \n\
	-i /data2/central_%Y%m%d_%H%M%S.dat\n\
	-i /data2/south_%Y%m%d_%H%M%S.dat\n\
	-i /data2/west_%Y%m%d_%H%M%S.dat\n\
	-m /data3/isff_%Y%m%d_%H%M%S.dat -r 10\n\
	-s \"2006 Apr 1 00:00\" -e \"2006 Apr 10 00:00\"\n\
" << endl;
    return 1;
}

/* static */
int MergeVerifier::main(int argc, char** argv) throw()
{
    setupSignals();

    MergeVerifier verifier;

    int res;
    
    if ((res = verifier.parseRunstring(argc,argv)) != 0) return res;

    return verifier.run();
}


MergeVerifier::MergeVerifier():
    inputFileNames(),mergeFileNames(),
    readAheadUsecs(30*USECS_PER_SEC),
    startTime(LONG_LONG_MIN),endTime(LONG_LONG_MIN),
    nmissing(0)
{
}

int MergeVerifier::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "-e:i:m:s:r:")) != -1) {
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
		fileNames.push_back(optarg);
		while(optind < argc && argv[optind][0] != '-') {
		    fileNames.push_back(argv[optind++]);
		}
		inputFileNames.push_back(fileNames);
	    }
	    break;
	case 'm':
	    mergeFileNames.push_back(optarg);
            while(optind < argc && argv[optind][0] != '-') {
                mergeFileNames.push_back(argv[optind++]);
            }
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
    if (mergeFileNames.size() == 0) return usage(argv[0]);
    return 0;
}

void MergeVerifier::printHeader(const SampleInputHeader& header)
{
    cerr << "ArchiveVersion:" << header.getArchiveVersion() << endl;
    cerr << "SoftwareVersion:" << header.getSoftwareVersion() << endl;
    cerr << "ProjectName:" << header.getProjectName() << endl;
    cerr << "SystemName:" << header.getSystemName() << endl;
    cerr << "ConfigName:" << header.getConfigName() << endl;
    cerr << "ConfigVersion:" << header.getConfigVersion() << endl;
}

void MergeVerifier::reportMissing(SampleInputStream* input,
        SampleInputStream* merge, Sample* samp)
{
    n_u::UTime tt(samp->getTimeTag());
    cerr << "Missing sample, in=" << input->getName() <<
    	", merge=" << merge->getName() << 
	": " << tt.format(true,"%Y %m %d %H:%M:%S.%6f") <<
        ", id=" << GET_DSM_ID(samp->getId()) << ',' <<
        GET_SHORT_ID(samp->getId()) <<
        ", len=" << samp->getDataByteLength() << endl;
    nmissing++;
}

void MergeVerifier::reportBackward(int nback,SampleInputStream* input,
        Sample* samp)
{
    n_u::UTime tt(samp->getTimeTag());
    cerr << "Backward sample (#" << nback << "), in=" << input->getName() <<
	": " << tt.format(true,"%Y %m %d %H:%M:%S.%6f") <<
        ", id=" << GET_DSM_ID(samp->getId()) << ',' <<
        GET_SHORT_ID(samp->getId()) <<
        ", len=" << samp->getDataByteLength() << endl;
    nmissing++;
}

void MergeVerifier::reportDuplicate(unsigned ndup, SampleInputStream* merge, Sample* samp)
{
    n_u::UTime tt(samp->getTimeTag());
    cerr << "Duplicate sample (#" << ndup << ") in=" << merge->getName() <<
	": " << tt.format(true,"%Y %m %d %H:%M:%S.%6f") <<
        ", id=" << GET_DSM_ID(samp->getId()) << ',' <<
        GET_SHORT_ID(samp->getId()) <<
        ", len=" << samp->getDataByteLength() << endl;
}

int MergeVerifier::run() throw()
{

    try {

	vector<SampleInputStream*> inputs;
        vector<bool> ieof(inputFileNames.size(),false);
        SampleInputStream* merge = 0;

	for (unsigned int ii = 0; ii < inputFileNames.size(); ii++) {

	    const list<string>& inputFiles = inputFileNames[ii];

	    nidas::core::FileSet* fset = new nidas::core::FileSet();
	    fset->setStartTime(startTime);
	    fset->setEndTime(endTime);

	    list<string>::const_iterator fi = inputFiles.begin();
	    for (; fi != inputFiles.end(); ++fi) {
		if (inputFiles.size() == 1 && 
			fi->find('%') != string::npos) fset->setFileName(*fi);
		else fset->addFileName(*fi);
	    }
#ifdef DEBUG
            cerr << "getName=" << fset->getName() << endl;
	    cerr << "start time=" << startTime.format(true,"%c") << endl;
	    cerr << "end time=" << endTime.format(true,"%c") << endl;
#endif

	    // SampleInputStream owns the iochan ptr.
	    SampleInputStream* input = new SampleInputStream(fset);
            input->setMaxSampleLength(32768);
            n_u::UTime filter1(startTime - USECS_PER_DAY);
            n_u::UTime filter2(endTime + USECS_PER_DAY);
            input->setMinSampleTime(filter1);
            input->setMaxSampleTime(filter2);
	    inputs.push_back(input);

	    // input->init();

	    try {
		input->readInputHeader();
		// SampleInputHeader header = input->getInputHeader();
		// printHeader(header);
	    }

	    catch (const n_u::EOFException& e) {
		cerr << input->getName() << ": " << e.what() << endl;
                ieof[ii] = true;
	    }
#ifdef CATCH_IO_E
	    catch (const n_u::IOException& e) {
		if (e.getErrno() != ENOENT) throw e;
		cerr << e.what() << endl;
                ieof[ii] = true;
	    }
#endif
	}
        nidas::core::FileSet* fset;

        list<string>::const_iterator fi = mergeFileNames.begin();

        if (mergeFileNames.size() == 1 && fi->find('%') != string::npos) {
#ifdef HAS_BZLIB_H
            if (fi->find(".bz2") != string::npos)
                fset = new nidas::core::Bzip2FileSet();
            else
#endif
                fset = new nidas::core::FileSet();
            fset->setFileName(*fi);
            fset->setStartTime(startTime);
            fset->setEndTime(endTime);
        }
        else fset = nidas::core::FileSet::getFileSet(mergeFileNames);
#ifdef DEBUG
        cerr << "getName=" << fset->getName() << endl;
        cerr << "start time=" << startTime.format(true,"%c") << endl;
        cerr << "end time=" << endTime.format(true,"%c") << endl;
#endif

        // SampleInputStream owns the iochan ptr.
        merge = new SampleInputStream(fset);
        bool eof = false;

        try {
            merge->readInputHeader();
	    SampleInputHeader header = merge->getInputHeader();
	    printHeader(header);
        }
        catch (const n_u::EOFException& e) {
            cerr << merge->getName() << ": " << e.what() << endl;
            eof = true;
        }

        Sample* msamp = 0;
        vector<Sample*> isamps(inputs.size(),0);
	unsigned int neof = 0;
	SampleT<char> dummy;
	SortedSampleSet3 mSamps;
	unsigned int ndups = 0;

	for (dsm_time_t tcur = startTime.toUsecs();
		neof < inputs.size() && !interrupted && tcur <= endTime.toUsecs();
		tcur += readAheadUsecs) {
            try {
                while (!interrupted & !eof) {
		    if (msamp) {
			if (msamp->getTimeTag() >= tcur + readAheadUsecs) break;
			if (!mSamps.insert(msamp).second) {
			    if (!(ndups++ % 200))
			    	reportDuplicate(ndups,merge,msamp);
			    msamp->freeReference();
			}
		    }
                    msamp = merge->readSample();
                }
            }
            catch (const n_u::EOFException& e) {
		cerr << merge->getName() << ": " << e.what() << endl;
                eof = true;
            }
	    if (mSamps.size() > 0) {
		n_u::UTime t1(tcur);
		n_u::UTime t2(tcur+readAheadUsecs);
		cout << t1.format(true,"%Y %m %d %H:%M:%S.%6f") << " - " <<
			t2.format(true,"%H:%M:%S.%6f") <<
			", merge samps=" << mSamps.size();
	    }
	    for (unsigned int ii = 0; ii < inputs.size(); ii++) {
		SampleInputStream* input = inputs[ii];
		Sample* samp = isamps[ii];
		int nok = 0;
		int nbad = 0;
		int nback = 0;
		while (!ieof[ii] && !interrupted) {
		    if (samp) {
			if (samp->getTimeTag() < tcur - readAheadUsecs) {
			    if (!(nback++ % 100))
				reportBackward(nback,input,samp);
			}
			else if (samp->getTimeTag() >= tcur + readAheadUsecs) break;
			else if (mSamps.find(samp) == mSamps.end()) {
			    reportMissing(input,merge,samp);
			    nbad++;
			}
			else nok++;
			samp->freeReference();
		    }
		    try {
			samp = input->readSample();
		    }
		    catch (const n_u::EOFException& e) {
			cerr << input->getName() << ": " << e.what() << endl;
			samp = 0;
			ieof[ii] = true;
			neof++;
		    }
#ifdef CATCH_IO_E
		    catch (const n_u::IOException& e) {
			if (e.getErrno() != ENOENT) throw e;
			cerr << e.what() << endl;
			ieof[ii] = true;
		    }
#endif
		}
		if (mSamps.size() > 0)
		    cout << ", " << ii << '=' << nok << ',' << nbad;
		isamps[ii] = samp;
	    }
	    if (mSamps.size() > 0) cout << endl;

	    dummy.setTimeTag(tcur);
	    SortedSampleSet3::const_iterator rsb = mSamps.begin();

	    // get iterator pointing at first sample not less than dummy
	    SortedSampleSet3::const_iterator rsi = mSamps.lower_bound(&dummy);

            for (SortedSampleSet3::const_iterator mi = rsb; mi != rsi; ++mi) {
                const Sample* samp = *mi;
                samp->freeReference();
            }
	    mSamps.erase(rsb,rsi);
	}
	for (SortedSampleSet3::const_iterator mi = mSamps.begin(); mi != mSamps.end(); ++mi) {
	    const Sample* samp = *mi;
	    samp->freeReference();
	}
	mSamps.clear();
	delete merge;
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
