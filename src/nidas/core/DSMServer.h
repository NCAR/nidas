// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#ifndef NIDAS_CORE_DSMSERVER_H
#define NIDAS_CORE_DSMSERVER_H

#include <nidas/core/DOMable.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/XMLException.h>
#include <nidas/util/SocketAddress.h>

#include <list>

namespace nidas { namespace util {
    class Thread;
}}

namespace nidas { namespace core {

class Project;
class Site;
class DSMService;

/**
 * A provider of services to a DSM.
 */
class DSMServer: public DOMable {
public:

    DSMServer();

    virtual ~DSMServer();

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    const std::string& getXMLConfigFileName() const { return _xmlFileName; }

    void setXMLConfigFileName(const std::string& val) { _xmlFileName = val; }

    void addService(DSMService* service) { _services.push_back(service); }

    const std::list<DSMService*>& getServices() const { return _services; }

    void setProject(Project* val) { _project = val; }

    const Project* getProject() const { return _project; }

    void setSite(const Site* val) { _site = val; }

    const Site* getSite() const { return _site; }

    DSMServiceIterator getDSMServiceIterator() const;

    ProcessorIterator getProcessorIterator() const;

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

    void addThread(nidas::util::Thread* thrd);

    void scheduleServices(bool optionalProcessing) throw(nidas::util::Exception);

    void interruptServices() throw();

    void joinServices() throw();

    void setStatusSocketAddr(const nidas::util::SocketAddress& val)
    {
        delete _statusSocketAddr;
        _statusSocketAddr = val.clone();
    }

    const nidas::util::SocketAddress& getStatusSocketAddr() const
    {
        return *_statusSocketAddr;
    }

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

private:

    /**
     * Name of this server. This should correspond to a hostname
     * of a machine.
     */
    std::string _name;

    Project* _project;

    /**
     * What Site to I serve?  Can be NULL if this DSMServer is not for a specific Site,
     * but serves the Project.
     */
    const Site* _site;

    /**
     * The DSMServices that we've been configured to start.
     */
    std::list<DSMService*> _services;

    std::string _xmlFileName;

    nidas::util::SocketAddress* _statusSocketAddr;

    /**
     * Copy not supported.
     */
    DSMServer(const DSMServer&);

    /**
     * Assignment not supported.
     */
    DSMServer& operator=(const DSMServer&);

};

}}	// namespace nidas namespace core

#endif
