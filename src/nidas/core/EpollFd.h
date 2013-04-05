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

#ifndef NIDAS_CORE_EPOLLFD_H
#define NIDAS_CORE_EPOLLFD_H

namespace nidas { namespace core {

class SensorHandler;

/**
 * An object with an associated file descriptor that can be 
 * monitored with epoll.
 */
class EpollFd {
public:
    virtual ~EpollFd() {}
    virtual void handleEpollEvents(uint32_t events) throw() = 0;
};

}}	// namespace nidas namespace core

#endif
