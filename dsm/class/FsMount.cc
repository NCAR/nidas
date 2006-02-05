/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-03 15:02:26 -0700 (Sat, 03 Dec 2005) $

    $LastChangedRevision: 3176 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/FsMount.cc $
 ********************************************************************

*/

#include <FsMount.h>

#include <fstream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(FsMount)

FsMount::FsMount() : type("auto") {}

void FsMount::mount()
       throw(atdUtil::IOException)
{
    if (isMounted()) return;

    if (::mount(getDevice().c_str(),getDir().c_str(),
    	getType().c_str(),0,getOptions().c_str()) < 0)
	throw atdUtil::IOException(
	    string("mount ") + getDevice() + " -t " + getType() +
	    (getOptions().length() > 0 ?  string(" -o ") + getOptions() : "") +
	    ' ' + getDir(),"failed",errno);
}

void FsMount::unmount()
       throw(atdUtil::IOException)
{
    if (!isMounted()) return;

    if (umount(getDir().c_str()) < 0)
	throw atdUtil::IOException(string("umount ") + getDir(),
		"failed",errno);
}

bool FsMount::isMounted() {
    ifstream mfile("/proc/mounts");

    for (;;) {
	string mdev,mpt;
	mfile >> mdev >> mpt;
	mfile.ignore(1000,'\n');
	if (mfile.fail()) return false;
	if (mfile.eof()) return false;
	if (mpt == getDir()) return true;
    }
}

void FsMount::fromDOMElement(const DOMElement* node)
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
	    else if (!aname.compare("dev")) setDevice(aval);
	    else if (!aname.compare("type")) setType(aval);
	    else if (!aname.compare("options")) setOptions(aval);
	    else throw atdUtil::InvalidParameterException("mount",
			"unrecognized attribute", aname);
	}
    }
}

DOMElement* FsMount::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("mount"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* FsMount::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

