/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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
