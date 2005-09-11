
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_PSQLCHANNEL_H
#define DSM_PSQLCHANNEL_H

#include <libpq-fe.h>		// PostgreSQL front end

#include <IOChannel.h>

namespace dsm {

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
    IOChannel* clone() const;

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    void requestConnection(ConnectionRequester*,int pseudoPort)
        throw(atdUtil::IOException);
                                                                                
    IOChannel* connect(int pseudoPort) throw(atdUtil::IOException);
                                                                                
   /**
     * Read method (not used).
     */
    size_t read(void* buf, size_t len) throw(atdUtil::IOException) 
    {
        throw atdUtil::IOException(getName(),"read","not supported");
    }
                                                                                
    /**
     * Write method. Sends SQL command to DB.
     */
    size_t write(const void* buf, size_t len)
        throw(atdUtil::IOException);
                                                                                
    void flush() throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);

    int getFd() const { return -1; }

    void setHost(const std::string& val);

    const std::string& getHost() const { return host; }

    void setDBName(const std::string& val);

    const std::string& getDBName() const { return dbname; }

    void setUser(const std::string& val);

    const std::string& getUser() const { return user; }

    void fromDOMElement(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);

    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

protected:

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

}

#endif

