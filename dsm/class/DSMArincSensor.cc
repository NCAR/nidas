/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMArincSensor.cc $

 ******************************************************************
*/


#include <arinc.h>
#include <DSMArincSensor.h>
#include <RTL_DevIoctlStore.h>

#include <asm/ioctls.h>

#include <iostream>
#include <sstream>

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_ENTRY_POINT(DSMArincSensor)

DSMArincSensor::DSMArincSensor()
{
  cerr << __PRETTY_FUNCTION__ << endl;
  for (int lbl=0; lbl<0400; lbl++)
    labelRate[lbl] = 0;
}

DSMArincSensor::DSMArincSensor(const string& nameArg) :
    RTL_DSMSensor(nameArg)
{
  cerr << __PRETTY_FUNCTION__ << "(" << nameArg << ")" << endl;
  for (int lbl=0; lbl<0400; lbl++)
    labelRate[lbl] = 0;
}

DSMArincSensor::~DSMArincSensor() {
  try {
    close();
  }
  catch(atdUtil::IOException& ioe) {
    cerr << __PRETTY_FUNCTION__ << ": " << ioe.what() << endl;
  }
}

void DSMArincSensor::open(int flags) throw(atdUtil::IOException)
{
  cerr << __PRETTY_FUNCTION__ << "jdw-begin" << endl;

  RTL_DSMSensor::open(flags);

  ioctl(ARINC_RESET, (const void*)0,0);

  arcfg_t arcfg;
  for (int lbl=0; lbl<0400; lbl++)
    if (labelRate[lbl]) {
      arcfg.label = lbl;
      arcfg.rate  = labelRate[lbl];
      cerr << "arcfg.label = " << arcfg.label << endl;
      cerr << "arcfg.rate  = " << arcfg.rate  << endl;
      ioctl(ARINC_SET, &arcfg, sizeof(arcfg_t));
    }
  
  ioctl(ARINC_STAT, (const void*)0,0);
  ioctl(ARINC_GO,(const void*)0,0);
  cerr << __PRETTY_FUNCTION__ << "jdw-end" << endl;
}

void DSMArincSensor::close() throw(atdUtil::IOException)
{
  cerr << __PRETTY_FUNCTION__ << endl;

  ioctl(ARINC_RESET, (const void*)0,0);
  RTL_DSMSensor::close();
}

void DSMArincSensor::fromDOMElement(const DOMElement* node)
  throw(atdUtil::InvalidParameterException)
{
  RTL_DSMSensor::fromDOMElement(node);
  XDOMElement xnode(node);

  cerr << __PRETTY_FUNCTION__ << ": xnode element name: " <<
    xnode.getNodeName() << endl;
	
  DOMNode* child;
  for (child = node->getFirstChild(); child != 0;
       child = child->getNextSibling())
  {
    if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
    XDOMElement xchild((DOMElement*) child);
    const string& elname = xchild.getNodeName();

    if (!elname.compare("arcfg")) {

      if ( xchild.getAttributeValue("label").c_str()[0] != '0' )
        throw atdUtil::InvalidParameterException (__PRETTY_FUNCTION__,
          "not an octal label:", xchild.getAttributeValue("label").c_str());

      unsigned char label = strtoul(xchild.getAttributeValue("label").c_str(),NULL,0);
      unsigned char rate  = strtoul(xchild.getAttributeValue("rate").c_str(),NULL,0);

      cerr << " labl: " << xchild.getAttributeValue("label")
           << " rate: " << xchild.getAttributeValue("rate")
           << " desc: " << xchild.getAttributeValue("desc")  << endl;

      if ( labelRate[label] )
        throw atdUtil::InvalidParameterException (__PRETTY_FUNCTION__,
          "duplicate label configured:", xchild.getAttributeValue("label").c_str());

      labelRate[label] = rate;
    }
  }
}
