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


#ifndef NIDAS_CORE_PORTSELECTORTEST_H
#define NIDAS_CORE_PORTSELECTORTEST_H

#include <csignal>

#include <nidas/core/SensorHandler.h>

namespace nidas { namespace core {

class PortSelectorTest : public SensorHandler {
public:
    static PortSelectorTest* getInstance();
    static PortSelectorTest* createInstance();
    static void sigAction(int sig,siginfo_t* siginfo, void* vptr);
    ~PortSelectorTest();
private:
    PortSelectorTest();
    static PortSelectorTest* instance;

};

}}	// namespace nidas namespace core

#endif
