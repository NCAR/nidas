/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <FileSet.h>
#include <DSMConfig.h>
#include <DSMService.h>
#include <Aircraft.h>

using namespace dsm;
using namespace std;

const std::string& FileSet::getName() const
{
    return name;
}

void FileSet::setName(const std::string& val)
{
    name = val;
}

void FileSet::requestConnection(ConnectionRequester* requester,int pseudoPort)
       throw(atdUtil::IOException)
{
   // immediate connection
   requester->connected(this); 
}

void FileSet::setDir(const string& val)
{
    atdUtil::FileSet::setDir(expandString(val));
}
    
void FileSet::setFileName(const string& val)
{
    atdUtil::FileSet::setFileName(expandString(val));
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
}
    
string FileSet::expandString(const string& input)
{
    string::size_type lastpos = 0;
    string::size_type dollar;

    string result;

    while ((dollar = input.find('$',lastpos)) != string::npos) {

        result.append(input.substr(lastpos,dollar-lastpos));
	lastpos = dollar;

	string::size_type openparen = input.find('{',dollar);
	if (openparen != dollar + 1) break;

	string::size_type closeparen = input.find('}',openparen);
	if (closeparen == string::npos) break;

	string token = input.substr(openparen+1,closeparen-openparen-1);
	if (token.length() > 0) {
	    string val = getTokenValue(token);
	    cerr << "getTokenValue: token=" << token << " val=" << val << endl;
	    result.append(val);
	}
	lastpos = closeparen + 1;
    }

    result.append(input.substr(lastpos));
    cerr << "input: \"" << input << "\" expanded to \"" <<
    	result << "\"" << endl;
    return result;
}

string FileSet::getTokenValue(const string& token)
{
    if (!token.compare("PROJECT")) {
	if (getDSMConfig())
	    return getDSMConfig()->getAircraft()->getProject()->getName();
	else if (getDSMService())
	    return getDSMService()->getAircraft()->getProject()->getName();
	else return "unknown";
    }
        
    if (!token.compare("AIRCRAFT")) {
	if (getDSMConfig())
	    return getDSMConfig()->getAircraft()->getName();
	else if (getDSMService())
	    return getDSMService()->getAircraft()->getName();
	else return "unknown";
    }
        
    if (!token.compare("LOCATION")) {
	if (getDSMConfig()) return getDSMConfig()->getLocation();
	else return "unknown";
    }

    // if none of the above, try to get token value from UNIX environment
    const char* val = ::getenv(token.c_str());
    if (val) return string(val);
    else return "unknown";
}

void FileSet::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
	// get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("dir")) setDir(aval);
	    else if (!aname.compare("file")) setFileName(aval);
	}
    }
}

xercesc::DOMElement* FileSet::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* FileSet::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

