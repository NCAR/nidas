// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2014-08-11 14:44:53 -0600 (Mon, 11 Aug 2014) $

    $LastChangedRevision: 7095 $

    $LastChangedBy: granger $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/raf/SyncRecordReader.h $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAF_SYNCSERVER_H
#define NIDAS_DYNLD_RAF_SYNCSERVER_H

#include <list>
#include <string>
#include <memory>

#include <nidas/core/Socket.h>

namespace nidas { namespace dynld { namespace raf {

class SyncServer
{
public:

    SyncServer();

    int run() throw(nidas::util::Exception);

    void
    setSorterLengthSeconds(float sorter_secs)
    {
        _sorterLengthSecs = sorter_secs;
    }

    void
    setXMLFileName(const std::string& name)
    {
        _xmlFileName = name;
    }        

    void
    interrupt(bool interrupted)
    {
        _interrupted = interrupted;
    }

    void
    resetAddress(nidas::util::SocketAddress* addr)
    {
        _address.reset(addr);
    }

    void
    setDataFileNames(const std::list<std::string>& dataFileNames)
    {
        _dataFileNames = dataFileNames;
    }

    static const int DEFAULT_PORT = 30001;

    static const float SORTER_LENGTH_SECS = 2.0;

private:

    std::string _xmlFileName;

    std::list<std::string> _dataFileNames;

    std::auto_ptr<nidas::util::SocketAddress> _address;

    float _sorterLengthSecs;

    bool _interrupted;
};

}}}	// namespace nidas namespace dynld namespace raf


#endif // NIDAS_DYNLD_RAF_SYNCSERVER_H


