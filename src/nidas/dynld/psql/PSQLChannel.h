// -*- mode: c++; c-basic-offset: 4; -*- 
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

