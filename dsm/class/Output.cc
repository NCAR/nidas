/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <McSocketAccepterOutput.h>
#include <McSocketRequesterOutput.h>
#include <FileSetOutput.h>

using namespace dsm;
using namespace std;

void Output::offer(atdUtil::Socket* sock) throw(atdUtil::Exception)
{
    throw atdUtil::Exception("output object is not a socket");
}

/* static */

Output* Output::fromOutputDOMElement(const xercesc::DOMElement* node)
            throw(atdUtil::InvalidParameterException)
{
    
    XDOMElement xnode((xercesc::DOMElement*) node);
    const string& elname = xnode.getNodeName();
    Output* output = 0;

    if (!elname.compare("socket")) {
	const string& typeattr = xnode.getAttributeValue("type");
	if (!typeattr.compare("mcaccept"))
	    output = new McSocketAccepterOutput();
	else if (!typeattr.compare("mcrequest"))
	    output = new McSocketRequesterOutput();
	else throw atdUtil::InvalidParameterException(
	    "Output::fromOutputDOMElement","socket type not supported",typeattr);
    }
    else if (!elname.compare("fileset")) output = new FileSetOutput();
    else throw atdUtil::InvalidParameterException(
	    "Output::fromDOMElement","output","only <socket> or <fileset> tags allowed");
    if(!output) throw atdUtil::InvalidParameterException(
	    "Output::fromDOMElement","output","no <socket> or <fileset> tags found");
    output->fromDOMElement((xercesc::DOMElement*)node);
    return output;
}

