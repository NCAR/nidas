// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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
