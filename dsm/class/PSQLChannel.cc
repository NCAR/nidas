
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <PSQLChannel.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(PSQLChannel)

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

void PSQLChannel::requestConnection(ConnectionRequester* requester,
	int pseudoPort) throw(atdUtil::IOException)
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
        atdUtil::IOException ioe(getName(),"PQconnectdb",PQerrorMessage(_conn));
	PQfinish(_conn);
	_conn = 0;
	throw ioe;
    }

    PQsetnonblocking(_conn, true);

    requester->connected(this);
}

IOChannel* PSQLChannel::connect(int pseudoPort) throw(atdUtil::IOException)
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
        atdUtil::IOException ioe(getName(),"PQconnectdb",PQerrorMessage(_conn));
	PQfinish(_conn);
	_conn = 0;
	throw ioe;
    }
										
    PQsetnonblocking(_conn, true);

    return clone();
}

void PSQLChannel::close() throw(atdUtil::IOException)
{
    if (_conn) {
	flush();
	PQfinish(_conn);
    }
    _conn = 0;
}

size_t PSQLChannel::write(const void* command, size_t len)
	throw(atdUtil::IOException)
{
    flush();

    if (!PQsendQuery(_conn, (const char*)command))
    	throw atdUtil::IOException(getName(),"PQSendQuery",
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

void PSQLChannel::flush() throw(atdUtil::IOException)
{
    if (_conn == 0) throw atdUtil::IOException(getName(),"write",
	    "not connected");
    PGresult* res;
    while ( (res = PQgetResult(_conn)) ) {
	ExecStatusType stat = PQresultStatus(res);
	if (stat == PGRES_FATAL_ERROR) {
	    atdUtil::IOException ioe(getName(),"PQsendQuery",
	    string(PQresultErrorMessage(res)) + ": " +
	    	(lastCommand ? string(lastCommand) : ""));
	    PQclear(res);
	    throw ioe;
	}
	PQclear(res);
    }
}

void PSQLChannel::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
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
	    else throw atdUtil::InvalidParameterException("postgresdb",
	    	aname,aval);
	}
    }
}

DOMElement* PSQLChannel::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* PSQLChannel::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


