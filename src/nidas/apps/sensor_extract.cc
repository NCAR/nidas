/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/RawSampleOutputStream.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Bzip2FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>

#include <csignal>
#include <climits>

#include <iomanip>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class SensorExtract: public HeaderSource
{
public:

    SensorExtract();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    void sendHeader(dsm_time_t,SampleOutput*)
        throw(n_u::IOException);
    
    /**
     * for debugging.
     */
    void printHeader();

private:

    static bool interrupted;

    list<string> inputFileNames;

    auto_ptr<n_u::SocketAddress> sockAddr;

    string outputFileName;

    int outputFileLength;

    SampleInputHeader header;

    set<dsm_sample_id_t> includeIds;

    set<dsm_sample_id_t> includeDSMIds;

    set<dsm_sample_id_t> excludeIds;

    set<int> excludeDSMIds;

    map<dsm_sample_id_t,dsm_sample_id_t> newIds;

    map<int,int> newDSMIds;

};

int main(int argc, char** argv)
{
    n_u::LogConfig lc;
    lc.level = n_u::LOGGER_INFO;
    n_u::Logger::getInstance()->setScheme
          (n_u::LogScheme("sensor_extract").addConfig (lc));
    return SensorExtract::main(argc,argv);
}


/* static */
bool SensorExtract::interrupted = false;

/* static */
void SensorExtract::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SensorExtract::interrupted = true;
    break;
    }
}

/* static */
void SensorExtract::setupSignals()
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
    act.sa_sigaction = SensorExtract::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int SensorExtract::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-s dsmids,sensorids[,newdsmid,newsensorid]] [-s ...]\n\
	[-x dsmid,sensorid] [-x ...] [-l output_file_length] output input ... \n\n\
    -s dsmids,sensorids[,newdsmid,newsensorid]:\n\
            Copy samples with ids matching the specified ids.\n\
            dsmids: a non-negative dsm id, or range of ids separated by a dash\n\
            sensorids: a sensor id, or range of ids separated by a dash, or\n\
                -1 for all sensors of a dsm\n\
	    newdsmid,newsensorid: change the id of samples that match dsmids,sensorids\n\
                to newdsmid,newsensorid.\n\
                If sensorid is -1, then only the dsm id can be changed to newdsmid\n\
            More than one -s option can be specified.\n\
    -x dsmids[,sensorids]:\n\
            Exclude samples with ids matching the specified ids.\n\
            dsmids: a non-negative dsm id, or range of ids separated by a dash\n\
            sensorids: a sensor id, a range of ids separated by a dash, \n\
                -1 for all sensors of a dsm. If sensorids is missing the default is -1.\n\
            More than one -x option can be specified\n\
    -l output_file_length: length of output files, in seconds\n\
    output: output file name or file name format\n\
    input ...: one or more input file name or file name formats, or\n\
        sock:[hostname:port]  to connect to a socket on hostname, or\n\
            hostname defaults to \"localhost\", port defaults to " <<
                NIDAS_RAW_DATA_PORT_TCP << "\n\
        unix:path to connect to a unix socket on the localhost\n\n\
    Either -s or -x options can be specified, but not both\n\
    Any id can start with 0x indicating a hex value, or 0, indicating\n\
    an octal value\n\
        \n\
" << endl;
    return 1;
}

/* static */
int SensorExtract::main(int argc, char** argv) throw()
{
    setupSignals();

    SensorExtract merge;

    int res;
    
    if ((res = merge.parseRunstring(argc,argv)) != 0) return res;

    return merge.run();
}


SensorExtract::SensorExtract():
    inputFileNames(),sockAddr(0),outputFileName(),
    outputFileLength(0),header(),
    includeIds(),includeDSMIds(),
    excludeIds(),excludeDSMIds(),
    newIds(),newDSMIds()
{
}

int SensorExtract::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "l:s:x:")) != -1) {
	switch (opt_char) {
	case 'l':
	    outputFileLength = atoi(optarg);
	    break;
	case 's':
            {
                const char* cp1 = optarg;
                const char* ep = cp1 + strlen(cp1);
                char* cp2;
                const char*d1,*d2;

                int dsmid1,dsmid2;
                int snsid1 = -1,snsid2 = -1;

                int newdsmid = -1;
                int newsnsid = -1;

                // strtol handles hex in the form 0xXXXX

                // parse dsm id field, with optional dash
                d1 = strchr(cp1,',');
                if (!d1) d1 = ep;
                while (::isspace(*cp1)) cp1++;
                if ((d2 = strchr(cp1+1,'-')) && d2 < d1) {
                    dsmid1 = strtol(cp1,&cp2,0);
                    if (cp2 != d2) return usage(argv[0]);
                    cp1 = d2 + 1;
                    dsmid2 = strtol(cp1,&cp2,0);
                }
                else {
                    dsmid1 = dsmid2 = strtol(cp1,&cp2,0);
                }
                if (cp2 != d1) return usage(argv[0]);

                if (d1 < ep) {
                    // parse sensor id field, with optional dash
                    cp1 = d1 + 1;
                    d1 = strchr(cp1,',');
                    if (!d1) d1 = ep;
                    while (::isspace(*cp1)) cp1++;
                    if ((d2 = strchr(cp1+1,'-')) && d2 < d1) {
                        snsid1 = strtol(cp1,&cp2,0);
                        if (cp2 != d2) return usage(argv[0]);
                        cp1 = d2 + 1;
                        snsid2 = strtol(cp1,&cp2,0);
                    }
                    else {
                        snsid1 = snsid2 = strtol(cp1,&cp2,0);
                    }
                    if (cp2 != d1) return usage(argv[0]);

                    // parse new dsm and sensor ids, no dashes
                    if (d1 < ep) {
                        cp1 = d1 + 1;
                        d1 = strchr(cp1,',');
                        if (!d1) d1 = ep;
                        newdsmid = strtol(cp1,&cp2,0);
                        if (cp2 != d1) return usage(argv[0]);
                        if (d1 < ep) {
                            cp1 = d1 + 1;
                            d1 = ep;
                            newsnsid = strtol(cp1,&cp2,0);
                            if (cp2 != d1) return usage(argv[0]);
                        }
                    }
                }
                if (cp2 != ep) return usage(argv[0]);

#ifdef DEBUG
                cerr << "dsmid1=" << dsmid1 << ", dsmid2=" << dsmid2 << ", snsid1=" << snsid1 << ", snsid2=" << snsid2 <<
                    ", newdsmid=" << newdsmid << ", newsnsid=" << newsnsid << endl;
#endif

                dsm_sample_id_t sampleId = 0;
                for (int did = dsmid1; did <= dsmid2; did++) {
                    if (did < 0) {
                        cerr << "ERROR: DSM id must be >= 0" << endl;
                        return usage(argv[0]);
                    }
                    sampleId = SET_DSM_ID(sampleId,did);
                    for (int sid = snsid1; sid <= snsid2; sid++) {
                        if (sid == -1) {  // all sample ids of this dsm
                            includeDSMIds.insert(did);
                            if (newdsmid < 0) newDSMIds[did] = did;
                            else newDSMIds[did] = newdsmid;
                        }
                        else {
                            sampleId = SET_SPS_ID(sampleId,sid);
                            includeIds.insert(sampleId);

                            dsm_sample_id_t newid = sampleId;    // by default, don't change
                            if (newdsmid >= 0) newid = SET_DSM_ID(newid,newdsmid);
                            if (newsnsid >= 0) newid = SET_SPS_ID(newid,newsnsid);
                            newIds[sampleId] = newid;
                        }
                    }
                }
            }
	    break;
        case 'x':
            {
                const char* cp1 = optarg;
                const char* ep = cp1 + strlen(cp1);
                char* cp2;
                const char*d1,*d2;

                int dsmid1,dsmid2;
                int snsid1=-1,snsid2=-1;

                // parse dsm id field, with optional dash
                d1 = strchr(cp1,',');
                if (!d1) d1 = ep;
                while (::isspace(*cp1)) cp1++;
                if ((d2 = strchr(cp1+1,'-')) && d2 < d1) {
                    dsmid1 = strtol(cp1,&cp2,0);
                    if (cp2 != d2) return usage(argv[0]);
                    cp1 = d2 + 1;
                    dsmid2 = strtol(cp1,&cp2,0);
                }
                else {
                    dsmid1 = dsmid2 = strtol(cp1,&cp2,0);
                }
                if (cp2 != d1) return usage(argv[0]);

                if (d1 < ep) {
                    // parse sensor id field, with optional dash
                    cp1 = d1 + 1;
                    d1 = strchr(cp1,',');
                    if (!d1) d1 = ep;
                    while (::isspace(*cp1)) cp1++;
                    if ((d2 = strchr(cp1+1,'-')) && d2 < d1) {
                        snsid1 = strtol(cp1,&cp2,0);
                        if (cp2 != d2) return usage(argv[0]);
                        cp1 = d2 + 1;
                        snsid2 = strtol(cp1,&cp2,0);
                    }
                    else {
                        snsid1 = snsid2 = strtol(cp1,&cp2,0);
                    }
                    if (cp2 != d1) return usage(argv[0]);
                }
                if (cp2 != ep) return usage(argv[0]);

                for (int did = dsmid1; did <= dsmid2; did++) {
                    if (did < 0) return usage(argv[0]);
                    for (int sid = snsid1; sid <= snsid2; sid++) {
                        if (sid == -1)  // all sample ids of this dsm
                            excludeDSMIds.insert(did);
                        else {
                            dsm_sample_id_t sampleId = 0;
                            sampleId = SET_DSM_ID(sampleId,did);
                            sampleId = SET_SPS_ID(sampleId,sid);
                            excludeIds.insert(sampleId);
                        }
                    }
                }

            }
            break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind < argc) outputFileName = argv[optind++];
    for ( ;optind < argc; )
        inputFileNames.push_back(argv[optind++]);
    if (inputFileNames.size() == 0) return usage(argv[0]);

    if (inputFileNames.size() == 1) {
        string url = inputFileNames.front();
        if (url.substr(0,5) == "sock:") {
            url = url.substr(5);
	    string hostName = "127.0.0.1";
            int port = NIDAS_RAW_DATA_PORT_TCP;
	    if (url.length() > 0) {
		size_t ic = url.find(':');
		hostName = url.substr(0,ic);
		if (ic < string::npos) {
		    istringstream ist(url.substr(ic+1));
		    ist >> port;
		    if (ist.fail()) {
			cerr << "Invalid port number: " << url.substr(ic+1) << endl;
			return usage(argv[0]);
		    }
		}
	    }
            try {
                n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
                sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
            }
            catch(const n_u::UnknownHostException& e) {
                cerr << e.what() << endl;
                return usage(argv[0]);
            }
	}
	else if (url.substr(0,5) == "unix:") {
	    url = url.substr(5);
            sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
    }
    if ((!includeIds.empty() || !includeDSMIds.empty() > 0) &&
            (!excludeIds.empty() || !excludeDSMIds.empty())) return usage(argv[0]);
    return 0;
}

void SensorExtract::sendHeader(dsm_time_t,SampleOutput* out)
    throw(n_u::IOException)
{
    printHeader();
    header.write(out);
}

void SensorExtract::printHeader()
{
    cerr << "ArchiveVersion:" << header.getArchiveVersion() << endl;
    cerr << "SoftwareVersion:" << header.getSoftwareVersion() << endl;
    cerr << "ProjectName:" << header.getProjectName() << endl;
    cerr << "SystemName:" << header.getSystemName() << endl;
    cerr << "ConfigName:" << header.getConfigName() << endl;
    cerr << "ConfigVersion:" << header.getConfigVersion() << endl;
}

int SensorExtract::run() throw()
{
    bool outOK = true;
    try {
	nidas::core::FileSet* outSet = 0;
        if (outputFileName.find(".bz2") != string::npos) {
#ifdef HAVE_BZLIB_H
            outSet = new nidas::core::Bzip2FileSet();
#else
            cerr << "Sorry, no support for Bzip2 files on this system" << endl;
            exit(1);
#endif
        }
        else
            outSet = new nidas::core::FileSet();

	outSet->setFileName(outputFileName);
	outSet->setFileLengthSecs(outputFileLength);

        SampleOutputStream outStream(outSet);
        outStream.setHeaderSource(this);

        IOChannel* iochan = 0;

        if (sockAddr.get()) {

            n_u::Socket* sock = 0;
            for (int i = 0; !sock && !interrupted; i++) {
                try {
                    sock = new n_u::Socket(*sockAddr.get());
                }
                catch(const n_u::IOException& e) {
                    if (i > 2)
                        n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: retrying",e.what());
                    sleep(10);
                }
            }
            iochan = new nidas::core::Socket(sock);
        }
        else {
            iochan = 
                nidas::core::FileSet::getFileSet(inputFileNames);
        }

        // RawSampleInputStream owns the iochan ptr.
        RawSampleInputStream input(iochan);

        input.setMaxSampleLength(32768);

        input.readInputHeader();
        // save header for later writing to output
        header = input.getInputHeader();

        n_u::UTime screenTime(true,2001,1,1,0,0,0);

        try {
            for (;;) {

                Sample* samp = input.readSample();
                if (interrupted) break;

                if (samp->getTimeTag() < screenTime.toUsecs()) continue;

                dsm_sample_id_t id = samp->getId();

		if (!includeIds.empty() || !includeDSMIds.empty()) {
		    if (includeIds.find(id) != includeIds.end()) {
			dsm_sample_id_t newid = newIds[id];
			samp->setId(newid);
			if (!(outOK = outStream.receive(samp))) break;
		    }
                    else if (includeDSMIds.find(GET_DSM_ID(id)) != includeDSMIds.end()) {
                        int dsm = GET_DSM_ID(id);
			int newdsm = newDSMIds[dsm];
                        id = SET_DSM_ID(id,newdsm);
			samp->setId(id);
			if (!(outOK = outStream.receive(samp))) break;
		    }
                }
		else {
                    int dsmid = GET_DSM_ID(id);
                    if (excludeIds.find(id) == excludeIds.end() &&
                                excludeDSMIds.find(dsmid) == excludeDSMIds.end()) 
			if (!(outOK = outStream.receive(samp))) break;
                }
                samp->freeReference();
            }
        }
        catch (n_u::EOFException& ioe) {
            cerr << ioe.what() << endl;
        }

	outStream.flush();
	outStream.close();
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    if (!outOK) return 1;
    return 0;
}

