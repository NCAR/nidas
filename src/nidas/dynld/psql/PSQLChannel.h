/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
// -*- mode: c++; c-basic-offset: 4; -*- 
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

#ifndef NIDAS_DYNLD_PSQL_PSQLCHANNEL_H
#define NIDAS_DYNLD_PSQL_PSQLCHANNEL_H

#include <libpq-fe.h>		// PostgreSQL front end

#include <nidas/core/IOChannel.h>

namespace nidas { namespace dynld { namespace psql {

using namespace nidas::core;

class PSQLChannel : public IOChannel
{
public:

    PSQLChannel();

    /**
     * Copy constructor.
     */
    PSQLChannel(const PSQLChannel&);

    virtual ~PSQLChannel();

    /**
     * Clone method.
     */
    PSQLChannel* clone() const;

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    void requestConnection(ConnectionRequester*)
        throw(nidas::util::IOException);
                                                                               
    IOChannel* connect() throw(nidas::util::IOException);
                                                                               
   /**
     * Read method (not used).
     */
    size_t read(void* buf, size_t len) throw(nidas::util::IOException) 
    {
        throw nidas::util::IOException(getName(),"read","not supported");
    }
                                                                                
    /**
     * Write method. Sends SQL command to DB.
     */
    size_t write(const void* buf, size_t len)
        throw(nidas::util::IOException);
                                                                                
    void flush() throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    int getFd() const { return -1; }

    void setHost(const std::string& val);

    const std::string& getHost() const { return host; }

    void setDBName(const std::string& val);

    const std::string& getDBName() const { return dbname; }

    void setUser(const std::string& val);

    const std::string& getUser() const { return user; }

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

protected:

    void
    connectDatabase();

    std::string name;

    std::string host;

    std::string dbname;

    std::string user;

    /**
     * Database connection pointer.
     */
    PGconn   *_conn;

    char* lastCommand;
    size_t lastNchars;
};

}}}

#endif // NIDAS_DYNLD_PSQL_PSQLCHANNEL_H

