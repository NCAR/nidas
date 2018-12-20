// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
/*

    Extract samples from a list of sensors from an archive.

*/

#include "Extract2D.h"

#include <fstream>
#include <sys/stat.h>

#include <unistd.h>
#include <getopt.h>

static const size_t DefaultMinimumNumberParticlesRequired = 5;


using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;


#include <csignal>
#include <climits>

#include <iomanip>

namespace n_u = nidas::util;


/* static */
void Extract2D::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            Extract2D::interrupted = true;
    break;
    }
}

/* static */
void Extract2D::setupSignals()
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
    act.sa_sigaction = Extract2D::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
bool Extract2D::interrupted = false;

int Extract2D::usage(const char* argv0)
{
/* maybe some day we want this option.
    -x dsmid,sensorid: the dsm id and sensor id of samples to exclude\n\
            more than one -x option can be specified\n\
	    either -s or -x options can be specified, but not both\n\
*/
    cerr << "\
Usage: " << argv0 << " [-x dsmid,sensorid] [-a] [-s] [-c] [-n #] output input ... \n\n\
    -a: copy all records, ignore all thresholds.\n\
    -s: generate diode count histogram along flight path.\n\
    -c: generate particle count histogram.\n\
    -n #: Minimum number of time slices required per record to\n\
            transfer to output file.\n\
    output: output file name or file name format\n\
    input ...: one or more input file name or file name formats\n\
" << endl;
    return 1;
}


Extract2D::Extract2D():
    outputHeader(true), outputDiodeCount(false), outputParticleCount(false),
    copyAllRecords(false), xmlFileName(), inputFileNames(), outputFileName(),
    outputFileLength(0), header(), includeIds(), excludeIds(), newids(),
    minNumberParticlesRequired(DefaultMinimumNumberParticlesRequired)
{
}

int Extract2D::parseRunstring(int argc, char** argv) throw()
{
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "acsn:")) != -1) {
	switch (opt_char) {
	case 'a':
	    copyAllRecords = true;
            break;
	case 'c':
	    outputParticleCount = true;
            break;
	case 'n':
	    minNumberParticlesRequired = atoi(::optarg);
            break;
	case 's':
	    outputDiodeCount = true;
            break;
/*
        case 'x':
            {
                unsigned long dsmid;
                unsigned long sensorid;
                int i;
                i = sscanf(optarg,"%ld,%ld",&dsmid,&sensorid);
                if (i < 2) return usage(argv[0]);
                dsm_sample_id_t id = 0;
                id = SET_DSM_ID(id,dsmid);
                id = SET_SHORT_ID(id,sensorid);
                excludeIds.insert(id);
            }
            break;
*/
	case '?':
	    return usage(argv[0]);
	}
    }

    if (::optind < argc) outputFileName = argv[::optind++];
    for ( ;::optind < argc; )
        inputFileNames.push_back(argv[::optind++]);
    if (inputFileNames.size() == 0) return usage(argv[0]);

    return 0;
}


void Extract2D::sendHeader(dsm_time_t, SampleOutput* out)
{
    header.write(out);
}

void Extract2D::setTimeStamp(P2d_rec & record, Sample *samp)
{
    struct tm t;
    int msecs;

    dsm_time_t tt = samp->getTimeTag();
    n_u::UTime samp_time(tt);
    samp_time.toTm(true, &t, &msecs);
    msecs /= 1000;

    record.hour = htons(t.tm_hour);
    record.minute = htons(t.tm_min);
    record.second = htons(t.tm_sec);
    record.year = htons(t.tm_year + 1900);
    record.month = htons(t.tm_mon + 1);
    record.day = htons(t.tm_mday);
    record.msec = htons(msecs);
}

void Extract2D::setTimeStamp(PADS_rec & record, Sample *samp)
{
    struct tm t;
    int msecs;

    dsm_time_t tt = samp->getTimeTag();
    n_u::UTime samp_time(tt);
    samp_time.toTm(true, &t, &msecs);
    msecs /= 1000;

    record.hour = (t.tm_hour);
    record.minute = (t.tm_min);
    record.second = (t.tm_sec);
    record.year = (t.tm_year + 1900);
    record.month = (t.tm_mon + 1);
    record.day = (t.tm_mday);
    record.msec = (msecs);
}

