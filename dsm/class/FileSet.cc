/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <FileSet.h>
#include <DSMConfig.h>
#include <Site.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(FileSet)

const std::string& FileSet::getName() const
{
    return name;
}

void FileSet::setName(const std::string& val)
{
    name = val;
}

void FileSet::setFileName(const string& val)
{
    atdUtil::FileSet::setFileName(val);
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
}

IOChannel* FileSet::connect(int pseudoPort)
       throw(atdUtil::IOException)
{

    // expand the file and directory names. We wait til now
    // because these may contain tokens that depend on the
    // DSM and we may not know it until now.
    setDir(expandString(getDir()));
    setFileName(expandString(getFileName()));
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
    return clone();
}

void FileSet::requestConnection(ConnectionRequester* requester,int pseudoPort)
       throw(atdUtil::IOException)
{
    // expand the file and directory names. We wait til now
    // because these may contain tokens that depend on the
    // DSM and we may not know it until now.
    setDir(expandString(getDir()));
    setFileName(expandString(getFileName()));
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
    requester->connected(this); 
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
	    // cerr << "getTokenValue: token=" << token << " val=" << val << endl;
	    result.append(val);
	}
	lastpos = closeparen + 1;
    }

    result.append(input.substr(lastpos));
    // cerr << "input: \"" << input << "\" expanded to \"" <<
    // 	result << "\"" << endl;
    return result;
}

string FileSet::getTokenValue(const string& token)
{
    if (!token.compare("PROJECT")) return Project::getInstance()->getName();
        
    const list<const DSMConfig*>& dsms = getDSMConfigs();
    if (!token.compare("AIRCRAFT") || !token.compare("SITE")) {
	if (dsms.size() > 0)
	    return dsms.front()->getSite()->getName();
	else return "unknown";
    }
        
    if (!token.compare("LOCATION")) {
	if (dsms.size() > 1) return "multiple_locations";
	else if (dsms.size() == 1) return dsms.front()->getLocation();
	else return "unknown";
    }

    // if none of the above, try to get token value from UNIX environment
    const char* val = ::getenv(token.c_str());
    if (val) return string(val);
    else return "unknown";
}

dsm_time_t FileSet::createFile(dsm_time_t t,bool exact)
	throw(atdUtil::IOException)
{
    return (dsm_time_t)atdUtil::FileSet::createFile(
    	(time_t)(t/USECS_PER_SEC),exact) * USECS_PER_SEC;
}

void FileSet::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
	// get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("dir")) setDir(aval);
	    else if (!aname.compare("file")) setFileName(aval);
	    else if (!aname.compare("length")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),
			"length", aval);
		setFileLengthSecs(val);
	    }
	    else throw atdUtil::InvalidParameterException(getName(),
			"unrecognized attribute", aname);
	}
    }
}

DOMElement* FileSet::toDOMParent(
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

DOMElement* FileSet::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

