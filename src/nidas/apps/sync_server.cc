// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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

#include <nidas/dynld/raf/SyncServer.h>
#include <nidas/util/Logger.h>

#include <unistd.h>
#include <getopt.h>

#include <iostream>
#include <sstream>

namespace n_u = nidas::util;

using nidas::dynld::raf::SyncServer;

namespace 
{
    int logLevel = n_u::LOGGER_INFO;
}


int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0 << " [-l sorterSecs] [-x xml_file] [-p port] raw_data_file ...\n\
    -l sorterSecs: length of sample sorter, in fractional seconds\n\
        default=" << (float)SyncServer::SORTER_LENGTH_SECS << "\n       \
    -L loglevel: set logging level, 7=debug,6=info,5=notice,4=warning,3=err,...\n\
        The default level is " << logLevel << "\n\
    -p port: sync record output socket port number: default="
              << SyncServer::DEFAULT_PORT << "\n\
    -x xml_file (optional), default: \n\
	$ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
	where <project>, <aircraft> and <flight> are read from the input data header\n\
    raw_data_file: names of one or more raw data files, separated by spaces\n\
" << std::endl;
    return 1;
}


int parseRunstring(SyncServer& sync, int argc, char** argv)
{
    int opt_char;     /* option character */
    std::list<std::string> dataFileNames;

    while ((opt_char = getopt(argc, argv, "L:l:p:x:")) != -1) {
	switch (opt_char) {
        case 'l':
            {
                std::istringstream ist(optarg);
                float sorter_secs;
		ist >> sorter_secs;
		if (ist.fail()) 
                    return usage(argv[0]);
                sync.setSorterLengthSeconds(sorter_secs);
            }
            break;
        case 'L':
            {
                std::istringstream ist(optarg);
		ist >> logLevel;
		if (ist.fail()) 
                    return usage(argv[0]);
            }
            break;
	case 'p':
	    {
                int port;
                std::istringstream ist(optarg);
		ist >> port;
		if (ist.fail()) 
                    sync.resetAddress(new n_u::UnixSocketAddress(optarg));
                else
                    sync.resetAddress(new n_u::Inet4SocketAddress(port));
	    }
	    break;
	case 'x':
            sync.setXMLFileName(optarg);
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    for (; optind < argc; ) 
        dataFileNames.push_back(argv[optind++]);
    if (dataFileNames.size() == 0)
        return usage(argv[0]);
    sync.setDataFileNames(dataFileNames);
    return 0;
}


SyncServer* signal_target = 0;

void sigAction(int sig, siginfo_t* siginfo, void*)
{
    std::cerr <<
        "received signal " << strsignal(sig) << '(' << sig << ')' <<
        ", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
        ", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
        ", si_code=" << (siginfo ? siginfo->si_code : -1) << std::endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        if (signal_target)
        {
            signal_target->interrupt();
        }
        break;
    }
}


void setupSignals(SyncServer& sync)
{
    signal_target = &sync;
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
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}


int main(int argc, char** argv)
{
    SyncServer sync;
    setupSignals(sync);

    int res;
    n_u::LogConfig lc;
    n_u::Logger* logger;
    
    if ((res = parseRunstring(sync, argc, argv)) != 0) return res;

    logger = n_u::Logger::createInstance(&std::cerr);
    lc.level = logLevel;

    logger->setScheme(n_u::LogScheme().addConfig (lc));

    try {
        return sync.run();
    }
    catch(const n_u::Exception&e ) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
