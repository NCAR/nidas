
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <DSMConfigServer.h>

#include <XMLFdFormatTarget.h>
#include <Datagrams.h>

#include <XMLStringConverter.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace atdUtil;

SendXMLConfig::SendXMLConfig(xercesc::DOMNode* n,Socket s)
    throw(atdUtil::IOException):
    Thread(string("SendXMLConfig:") + s.getInet4SocketAddress().toString()),
    sock(s),writer(sock.getInet4SocketAddress().getInet4Address()),node(n)
{
}

int SendXMLConfig::run() throw(atdUtil::Exception)
{

    XMLFdFormatTarget formatter(addr.toString(),sock.getFd());

    writer.writeNode(&formatter,*node);

    sock.close();
    return RUN_OK;
}

DSMConfigServer::DSMConfigServer(const std::string& xmlName) :
	Thread("DSMConfigServer"),xmlFileName(xmlName) {}

int DSMConfigServer::run() throw(atdUtil::Exception)
{
    try {
	cerr << "creating parser" << endl;
	XMLConfigParser* parser = new XMLConfigParser();

	parser->setDOMValidation(true);
	parser->setDOMValidateIfSchema(true);
	parser->setDOMNamespaces(true);
	parser->setXercesSchema(true);
	parser->setXercesSchemaFullChecking(true);
	parser->setDOMDatatypeNormalization(true);

	// true means we delete doc
	// false means doc is deleted by parser's destructor.
	parser->setXercesUserAdoptsDOMDocument(true);

	xercesc::DOMDocument* doc = 0;

	time_t xmlModTime = 0;

	// can't bind to a specific address, must bind to INADDR_ANY.
	MulticastSocket readsock(DSM_MULTICAST_PORT);
	readsock.joinGroup(Inet4Address::getByName(DSM_MULTICAST_ADDR));

	for (;;) {
	    ConfigDatagram dgram;
	    readsock.receive(dgram);
	    cerr << "received datagram, length=" << dgram.getLength() << endl;
	    cerr << "received datagram, port=" << dgram.getDSMListenPort() << endl;
	    // parse XML when modification time changes.
	    time_t newModTime = getFileModTime(xmlFileName);
	    if (newModTime > xmlModTime) {
		delete doc;	// delete old doc
		cerr << "parsing doc" << endl;
		doc = parser->parse(xmlFileName);
		cerr << "doc parsed" << endl;
		xmlModTime = newModTime;
	    }

	    unsigned short port = dgram.getDSMListenPort();
	    Inet4Address dsmAddr = dgram.getAddress();

	    Socket dsmsock(Inet4SocketAddress(dsmAddr,port));

	    // start a thread to deliver the configuration to the DSM
	    SendXMLConfig sender(doc,dsmsock);
	    sender.start();
	    sender.join();
	}
	delete doc;
	delete parser;
    }
    catch (const xercesc::DOMException& e) {
        throw atdUtil::Exception(
		(const char*)XMLStringConverter(e.getMessage()));
    }
    catch (const xercesc::XMLException& e) {
        throw atdUtil::Exception(
		(const char*)XMLStringConverter(e.getMessage()));
    }
    return 0;
}

/* static */
time_t DSMConfigServer::getFileModTime(const std::string&  name) throw(atdUtil::IOException) 
{
    struct stat filestat;
    if (stat(name.c_str(),&filestat) < 0)
	throw atdUtil::IOException(name,"stat",errno);

    return filestat.st_mtime;
}


