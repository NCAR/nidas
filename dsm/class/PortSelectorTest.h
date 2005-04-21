/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_PORTSELECTORTEST_H
#define DSM_PORTSELECTORTEST_H

#include <signal.h>

#include <PortSelector.h>

namespace dsm {

class PortSelectorTest : public dsm::PortSelector {
public:
    static PortSelectorTest* getInstance();
    static PortSelectorTest* createInstance();
    static void sigAction(int sig,siginfo_t* siginfo, void* vptr);
    ~PortSelectorTest();
private:
    PortSelectorTest();
    static PortSelectorTest* instance;

};

}

#endif
