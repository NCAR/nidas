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

/**
 * Enumeration of file descriptor polling methods supported by SensorHander.
 */
#define POLL_EPOLL_ET   0       /* epoll edge-triggered */
#define POLL_EPOLL_LT   1       /* epoll level-triggered */
#define POLL_PSELECT    2       /* pselect */
#define POLL_POLL       3       /* poll/ppoll */

/**
 * Select a POLLING_METHOD
 */
#define POLLING_METHOD POLL_EPOLL_ET

#include <sys/poll.h>

/*
 * epoll.h defines EPOLLIN, EPOLLERR, EPOLLHUP, EPOLLRDHUP
 * poll.h defines POLLIN, POLLERR, POLLHUP, POLLRDHUP.
 * As of glibc 2.12 and 2.16 they have equal values.
 *
 * Define local macros N_POLLIN, N_POLLERR, N_POLLHUP, N_POLLRDHUP so
 * things work independently of the headers used.
 */

#define N_POLLIN POLLIN
#define N_POLLERR POLLERR
#define N_POLLHUP POLLHUP

// POLLRDHUP is somewhat new (Linux 2.6.17)
#ifdef POLLRDHUP
#define N_POLLRDHUP POLLRDHUP
#else
#define N_POLLRDHUP POLLHUP
#endif

#if POLLING_METHOD == POLL_PSELECT
#include <sys/select.h>
#endif

#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
#include <sys/epoll.h>
#endif

namespace nidas { namespace core {

class SensorHandler;

/**
 * An object with an associated file descriptor that can be 
 * monitored with epoll.
 */
class Polled {
public:
    virtual ~Polled() {}

    /**
     * @return: true: read consumed all available data, false otherwise.
     */
#if POLLING_METHOD == POLL_EPOLL_ET
    virtual bool handlePollEvents(uint32_t events) throw() = 0;
#else
    virtual void handlePollEvents(uint32_t events) throw() = 0;
#endif
};

}}	// namespace nidas namespace core

#endif
