/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#ifndef DSM_OUTPUTFILESET_H
#define DSM_OUTPUTFILESET_H


#include <atdUtil/OutputFileSet.h>
#include <DOMable.h>


namespace dsm {

/**
 * A wrapper
 */
class DSMOutputFileSet: public atdUtil::OutputFileSet, public DOMable {

public:
    // DSMOutputFileSet();
    virtual ~DSMOutputFileSet() {}

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


};

}

#endif
