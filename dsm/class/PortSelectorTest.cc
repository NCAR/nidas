/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#include <PortSelectorTest.h>

#include <signal.h>
#include <iostream>

using namespace std;
using namespace dsm;

PortSelectorTest* PortSelectorTest::instance = 0;

PortSelectorTest* PortSelectorTest::getInstance()
{
    return instance;
}

PortSelectorTest* PortSelectorTest::createInstance()
{
    if (!instance) instance = new PortSelectorTest();
    return instance;
}

PortSelectorTest::PortSelectorTest()
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
    act.sa_sigaction = PortSelectorTest::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

PortSelectorTest::~PortSelectorTest()
{
    instance = 0;
}

void PortSelectorTest::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr << "received signal " << strsignal(sig) << "(" << sig << ")" <<
	" si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	" si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	" si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
										
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
	    PortSelectorTest::getInstance()->cancel();
    break;
    }
}
