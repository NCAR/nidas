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

#ifndef NIDAS_UTIL_IOEXCEPTION_H
#define NIDAS_UTIL_IOEXCEPTION_H

#include <string>
#include <nidas/util/Exception.h>

#include <cerrno>	// not used here, but by many users of IOException

namespace nidas { namespace util {

class IOException : public Exception {
protected:

    /**
     * Constructor used by sub-classes of IOException (e.g./ EOFException).
     */
    IOException(const std::string& etype,const std::string& device,const std::string& task,int err) :
        Exception(etype,device + ": " + task,err)
    {
    }

    /**
     * Constructor used by sub-classes of IOException (e.g./ EOFException).
     */
    IOException(const std::string& etype,const std::string& device,const std::string& task,const std::string& msg) :
        Exception(etype,device + ": " + task + ": " + msg)
    {
    }

public:

    /**
     * Create an IOException, passing a device name, task (e.g.\ "read" or "ioctl"),
     * and a message.
     */
    IOException(const std::string& device, const std::string& task, const std::string& msg):
        Exception("IOException", device + ": " + task + ": " + msg)
    {}

    /**
     * Create an IOException, passing a device name, task (e.g.\ "read" or "ioctl"),
     * and a message.
     */
    IOException(const std::string& task, const std::string& msg):
        Exception("IOException", task + ": " + msg)
    {}

    /**
     * Create an IOException, passing a device name, task (e.g.\ "read" or "ioctl"),
     * and an errno value.
     */
    IOException(const std::string& device, const std::string& task, int err):
        Exception("IOException", device + ": " + task,err) 
    {
    }

    /**
     * Copy constructor.
     */
    IOException(const IOException& e): Exception(e) {}

    /**
     * clone myself (a "virtual" constructor).
     */
    virtual Exception* clone() const {
        return new IOException(*this);
    }
};

}}	// namespace nidas namespace util

#endif
