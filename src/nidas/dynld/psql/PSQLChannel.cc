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

#include "PSQLChannel.h"

#include "psql_impl.h"

NIDAS_CREATOR_FUNCTION_NS(psql,PSQLChannel)

PSQLChannel::PSQLChannel(): _conn(0),lastCommand(0),lastNchars(0)
{
}

PSQLChannel::PSQLChannel(const PSQLChannel& x):
    name(x.name),host(x.host),dbname(x.dbname),user(x.user),
    _conn(0),lastCommand(0),lastNchars(0)
{
}

PSQLChannel::~PSQLChannel()
{
    if (_conn) PQfinish(_conn);
    delete [] lastCommand;
}

void PSQLChannel::setHost(const std::string& val)
{
    host = val;
    setName(user + "@" + host + ":" + dbname);
}

void PSQLChannel::setDBName(const std::string& val)
{
    dbname = val;
    setName(user + "@" + host + ":" + dbname);
}

void PSQLChannel::setUser(const std::string& val)
{
    user = val;
    setName(user + "@" + host + ":" + dbname);
}

PSQLChannel* PSQLChannel::clone() const
{
    return new PSQLChannel(*this);
}


void
PSQLChannel::
connectDatabase()
{
    string connectstr;
    if (getHost().length() > 0) connectstr += "host=" + getHost() + ' ';
    if (getDBName().length() > 0) connectstr += "dbname=" + getDBName() + ' ';
    if (getUser().length() > 0) connectstr += "user=" + getUser() + ' ';
    cerr << "connectstr=" << connectstr << endl;

    _conn = PQconnectdb(connectstr.c_str());

    /* check to see that the backend connection was successfully made
    */
    if (PQstatus(_conn) == CONNECTION_BAD) {
        nidas::util::IOException ioe(getName(),"PQconnectdb",
				     PQerrorMessage(_conn));
	PQfinish(_conn);
	_conn = 0;
	throw ioe;
    }

    PQsetnonblocking(_conn, true);
}


void
PSQLChannel::
requestConnection(ConnectionRequester* requester)
    throw(nidas::util::IOException)
{
    DLOG(("enter"));
    connectDatabase();
    requester->connected(this);
}


IOChannel* 
PSQLChannel::
connect() throw(nidas::util::IOException)
{
    DLOG(("enter"));
    connectDatabase();
    return clone();
}

void PSQLChannel::close() throw(nidas::util::IOException)
{
    if (_conn) {
	flush();
	PQfinish(_conn);
    }
    _conn = 0;
}

size_t PSQLChannel::write(const void* command, size_t len)
	throw(nidas::util::IOException)
{
    flush();

    if (!PQsendQuery(_conn, (const char*)command))
    	throw nidas::util::IOException(getName(),"PQSendQuery",
	    PQerrorMessage(_conn));

    if (len >= lastNchars) {
        delete [] lastCommand;
	lastCommand = new char[len+1];
	lastNchars = len+1;
    }
    memcpy(lastCommand,command,len);
    lastCommand[len] = 0;
    return len;
}

void PSQLChannel::flush() throw(nidas::util::IOException)
{
    if (_conn == 0) throw nidas::util::IOException(getName(),"write",
	    "not connected");
    PGresult* res;
    while ( (res = PQgetResult(_conn)) ) {
	ExecStatusType stat = PQresultStatus(res);
	if (stat == PGRES_FATAL_ERROR) {
	    nidas::util::IOException ioe(getName(),"PQsendQuery",
	    string(PQresultErrorMessage(res)) + ": " +
	    	(lastCommand ? string(lastCommand) : ""));
	    PQclear(res);
	    throw ioe;
	}
	PQclear(res);
    }
}

void PSQLChannel::fromDOMElement(const DOMElement* node)
	throw(nidas::util::InvalidParameterException)
{
    string stype;
    string saddr;
    string sport;

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("host")) setHost(aval);
	    else if (!aname.compare("dbname")) setDBName(aval);
	    else if (!aname.compare("user")) setUser(aval);
	    else throw nidas::util::InvalidParameterException("postgresdb",
	    	aname,aval);
	}
    }
    Logger* log = Logger::getInstance();
    log->log(LOG_INFO, "created PSQLChannel: host=%s, user=%s, dbname=%s",
	     getHost().c_str(), getUser().c_str(), getDBName().c_str());
}
