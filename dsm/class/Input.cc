/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <McSocketAccepterInput.h>
#include <McSocketRequesterInput.h>
#include <FileSetInput.h>

using namespace dsm;
using namespace std;

void Input::offer(atdUtil::Socket* sock) throw(atdUtil::IOException)
{
    throw atdUtil::IOException("input","offer","object is not a socket");
}

/* static */
Input* Input::fromInputDOMElement(const xercesc::DOMElement* node)
            throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode((xercesc::DOMElement*) node);
    const string& elname = xnode.getNodeName();
    Input* input = 0;

    if (!elname.compare("socket")) {
	const string& typeattr = xnode.getAttributeValue("type");
	if (!typeattr.compare("mcaccept"))
	    input = new McSocketAccepterInput();
	else if (!typeattr.compare("mcrequest"))
	    input = new McSocketRequesterInput();
	else throw atdUtil::InvalidParameterException(
	    "Input::fromInputDOMElement","socket type not supported",typeattr);
    }
    else if (!elname.compare("fileset")) input = new FileSetInput();
    else throw atdUtil::InvalidParameterException(
	    "Input::fromDOMElement","input","only <socket> or <fileset> tags allowed");
    if (!input) throw atdUtil::InvalidParameterException(
	    "Input::fromDOMElement","input","no <socket> or <fileset> tags found");
    input->fromDOMElement((xercesc::DOMElement*)node);
    return input;
}
