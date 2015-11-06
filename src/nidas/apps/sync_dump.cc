/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/* -*- mode: C++; c-basic-offset: 4; -*- */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include <ctime>

#include <nidas/core/FileSet.h>
#include <nidas/util/Logger.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/raf/SyncRecordReader.h>

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <vector>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

#ifdef SUPPORT_JSON_OUTPUT
#include <json/json.h>
#endif

class SyncDumper
{
public:

    SyncDumper();

    int parseRunstring(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    int run();

    static bool interrupted;

    void printHeader();

private:

    SyncDumper(const SyncDumper&);

    SyncDumper& operator=(const SyncDumper&);

    string _dataFileName;

    auto_ptr<n_u::SocketAddress> _sockAddr;

    static const int DEFAULT_PORT = 30001;

    string _varname;

    string _dumpHeader;
    string _dumpJSON;
};

SyncDumper::SyncDumper(): _dataFileName(),_sockAddr(0),_varname(),
			  _dumpHeader(),
			  _dumpJSON()
{
}

int SyncDumper::parseRunstring(int argc, char** argv)
{
    // extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "h:j:")) != -1) {
	switch (opt_char) {
	case 'h':
	    _dumpHeader = string(optarg);
	    break;
	case 'j':
	    _dumpJSON = string(optarg);
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind != argc - 2) return usage(argv[0]);

    _varname = string(argv[optind++]);

    string url(argv[optind++]);
    if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	url = url.substr(5);
	size_t ic = url.find(':');
	string hostName = url.substr(0,ic);
        int port = DEFAULT_PORT;
	if (ic < string::npos) {
	    istringstream ist(url.substr(ic+1));
	    ist >> port;
	    if (ist.fail()) {
		cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		return usage(argv[0]);
	    }
	}
        try {
            n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
            _sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
        }
        catch(const n_u::UnknownHostException& e) {
            cerr << e.what() << endl;
            return usage(argv[0]);
        }
    }
    else if (url.length() > 5 && !url.compare(0,5,"unix:")) {
        url = url.substr(5);
        _sockAddr.reset(new n_u::UnixSocketAddress(url));
    }
    else _dataFileName = url;
    return 0;
}

int SyncDumper::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-h <file>] [-j <file>] variable inputURL\n\
    var: a variable name\n\
    -h <file>  Print the header to <file>, where <file> can be - for stdout.\n\
    -j <file>  Dump all sync samples as JSON to the given <file>.\n\
    inputURL: data input (required). One of the following:\n\
        sock:host[:port]          (Default port is " << DEFAULT_PORT << ")\n\
        unix:sockpath             unix socket name\n\
        path                      one or more file names\n\
Examples:\n" <<
	argv0 << " DPRES /tmp/xxx.dat\n" <<
	argv0 << " DPRES file:/tmp/xxx.dat\n" <<
	argv0 << " DPRES sock:hyper:30001\n" << endl;
#ifndef SYNC_RECORD_JSON_OUTPUT
    cerr << "JSON output is not available in this build of sync_dump.\n";
#endif
    return 1;
}


void SyncDumper::printHeader()
{
    cout << "|--- date time -------|  bytes" << endl;
}

bool SyncDumper::interrupted = false;

void SyncDumper::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SyncDumper::interrupted = true;
    break;
    }
}

void SyncDumper::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
//    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                                                
    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = SyncDumper::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
//    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

int SyncDumper::run()
{
    IOChannel* iochan = 0;

    if (_dataFileName.length() > 0) {
        list<string> _dataFileNames;
        _dataFileNames.push_back(_dataFileName);
        nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(_dataFileNames);
	iochan = fset;
    }
    else {
	n_u::Socket* sock = new n_u::Socket(*_sockAddr.get());
	iochan = new nidas::core::Socket(sock);
    }

    // SyncRecordReader owns the iochan
    SyncRecordReader reader(iochan);
    ofstream json;

    if (_dumpHeader == "-")
    {
	cerr << reader.textHeader() << endl;
    }
    else if (_dumpHeader.length())
    {
	ofstream hout(_dumpHeader.c_str());
	const std::string& header = reader.textHeader();
	hout.write(header.c_str(), header.length());
	hout.close();
    }

#ifdef SYNC_RECORD_JSON_OUTPUT
    if (_dumpJSON.length())
    {
	json.open(_dumpJSON.c_str());
	write_sync_record_header_as_json(json, reader.textHeader());
    }
#endif

    cerr << "project=" << reader.getProjectName() << endl;
    cerr << "aircraft=" << reader.getTailNumber() << endl;
    cerr << "flight=" << reader.getFlightName() << endl;
    cerr << "SoftwareVersion=" << reader.getSoftwareVersion() << endl;

    size_t numValues = reader.getNumValues();
    cerr << "numValues=" << reader.getNumValues() << endl;

    const list<const SyncRecordVariable*>& vars = reader.getVariables();
    cerr << "num of variables=" << vars.size() << endl;

#ifdef USE_FIND_IF
    // predicate class which compares variable names against a string.
    class MatchName {
    public:
        MatchName(const string& name): _name(name) {}
	bool operator()(const SyncRecordVariable* v) const {
	    return _name.compare(v->getName()) == 0;
	}
    private:
	string _name;
    } matcher(_varname);

    list<const SyncRecordVariable*>::const_iterator vi =
	std::find_if(vars.begin(), vars.end(), matcher);
#else
    list<const SyncRecordVariable*>::const_iterator vi;
    for (vi = vars.begin(); vi != vars.end(); ++vi) {
        const SyncRecordVariable *var = *vi;
	if (_varname == var->getName()) break;
    }
#endif

    if (vi == vars.end()) {
        cerr << "Can't find variable " << _varname << endl;
	return 1;
    }

    const SyncRecordVariable* var = *vi;
    size_t varoffset = var->getSyncRecOffset();
    size_t lagoffset = var->getLagOffset();
    int irate = (int)ceil(var->getSampleRate());
    int deltatUsec = (int)rint(USECS_PER_SEC / var->getSampleRate());
    int vlen = var->getLength();


    dsm_time_t tt,ttlast=LONG_LONG_MIN;
    vector<double> rec(numValues);
    cout << var->getName() << " (" << var->getUnits() << ") \"" <<
    	var->getLongName() << "\"" << endl;

    try {
	for (;;) {
	    size_t len = reader.read(&tt,&rec.front(),numValues);
#ifdef SYNC_RECORD_JSON_OUTPUT
	    if (_dumpJSON.length())
	    {
		write_sync_record_data_as_json(json, tt, &(rec[0]),
					       numValues);
	    }
#endif
	    if (interrupted) {
		// reader.interrupt();
		break;
	    }
	    if (len == 0) continue;

	    // cout << "lag= " << rec[lagoffset] << endl;
	    if (!isnan(rec[lagoffset])) tt += (int) rec[lagoffset];
            if (tt <= ttlast) {
                cerr << "timetag=" <<
                    n_u::UTime(tt).format(true,"%F %T.%3f") <<
                    " is less than or equal to previous " <<
                    n_u::UTime(ttlast).format(true,"%Y %m %d %H:%M:%S.%3f") << endl;
            }

	    for (int i = 0; i < irate; i++) {
                ttlast = tt;
                cout << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f");
		for (int j = 0; j < vlen; j++)
		    cout << ' ' << rec[varoffset + i*vlen + j];
		cout << endl;
		tt += deltatUsec;
	    }
	}
    }
    catch (const n_u::IOException& e) {
        cerr << "SyncDumper::main: " << e.what() << endl;
    }
#ifdef SYNC_RECORD_JSON_OUTPUT
    if (_dumpJSON.length())
    {
        json.close();
    }
#endif
    return 0;
}

int main(int argc, char** argv)
{
    SyncDumper::setupSignals();

    SyncDumper dumper;

    int res;
    n_u::LogConfig lc;
    n_u::Logger* logger;

    if ((res = dumper.parseRunstring(argc,argv)) != 0) return res;

    // Send all logging to cerr.
    logger = n_u::Logger::createInstance(&std::cerr);
    lc.level = n_u::LOGGER_DEBUG;

    logger->setScheme(n_u::LogScheme().addConfig (lc));
    return dumper.run();
}
