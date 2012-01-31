#include <nidas/core/requestXMLConfig.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLConfigInput.h>
#include <nidas/core/XMLFdInputSource.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;

/* static */
xercesc::DOMDocument* reqXMLconf::requestXMLConfig(const nidas::util::Inet4SocketAddress &mcastAddr, sigset_t *signalMask)
 throw(nidas::util::Exception)
{
    DLOG(("requestXMLConfig"));
 
    std::auto_ptr<XMLParser> parser(new XMLParser());
    // throws XMLException
 
    // If parsing xml received from a server over a socket,
    // turn off validation - assume the server has validated the XML.
    parser->setDOMValidation(false);
    parser->setDOMValidateIfSchema(false);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(false);
    parser->setXercesSchemaFullChecking(false);
    parser->setDOMDatatypeNormalization(false);

    XMLConfigInput xmlRequestSocket;
    xmlRequestSocket.setInet4McastSocketAddress(mcastAddr);

    std::auto_ptr<nidas::util::Socket> configSock;
    nidas::util::Inet4PacketInfoX pktinfo;

    try {
        if ( signalMask != (sigset_t*)0 )
        {
            DLOG(("requestXMLConfig: signalMask =") << signalMask);
            pthread_sigmask(SIG_UNBLOCK,signalMask,0);
        }
        configSock.reset(xmlRequestSocket.connect(pktinfo));
        if ( signalMask != (sigset_t*)0 )
            pthread_sigmask(SIG_BLOCK,signalMask,0);
    }
    catch(...) {
        if ( signalMask != (sigset_t*)0 )
            pthread_sigmask(SIG_BLOCK,signalMask,0);
        xmlRequestSocket.close();
        throw;
    }
    xmlRequestSocket.close();

    xercesc::DOMDocument* doc = 0;
    try {
        std::string sockName = configSock->getRemoteSocketAddress().toString();
        DLOG(("requestXMLConfig: sockName: ") << sockName);
        DLOG(("requestXMLConfig: configSock->getFd(): ") << configSock->getFd());
        DLOG(("requestXMLConfig: configSock->getRemotePort(): ") << configSock->getRemotePort());

        XMLFdInputSource sockSource(sockName,configSock->getFd());
        doc = parser->parse(sockSource);
        configSock->close();
    }
    catch(const nidas::util::IOException& e) {
        PLOG(("requestXMLConfig:") << e.what());
        configSock->close();
        throw e;
    }
    catch(const nidas::core::XMLException& xe) {
        PLOG(("requestXMLConfig:") << xe.what());
        configSock->close();
        throw xe;
    }
    catch(...) {
        configSock->close();
        throw;
    }
    return doc;
}
