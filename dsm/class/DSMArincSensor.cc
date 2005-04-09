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

#include <math.h>
#include <asm/ioctls.h>
#include <iostream>
#include <sstream>

using namespace std;
using namespace dsm;
using namespace xercesc;

//CREATOR_ENTRY_POINT(DSMArincSensor);

DSMArincSensor::DSMArincSensor() :  _nanf(nanf("")), sim_xmit(false)
{
}

DSMArincSensor::~DSMArincSensor() {
  try {
    close();
  }
  catch(atdUtil::IOException& ioe) {
    err(": %s", ioe.what() );
  }
//   arcfgs.clear();
//   labels.clear();
}

void DSMArincSensor::open(int flags) throw(atdUtil::IOException)
{
  err("");

  ioctl(ARINC_RESET, (const void*)0,0);

  // sort SampleTags by rate then by label
  set <const SampleTag*, SortByRateThenLabel> sortedSampleTag
    ( getSampleTags().begin(), getSampleTags().end() );

  if (sim_xmit) {
    err(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> transmitting");
    ioctl(ARINC_SIM_XMIT,(const void*)0,0);
  }
  else
    err("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< receiving");

  arcfg_t arcfg;
  for (set<const SampleTag*>::const_iterator si = sortedSampleTag.begin();
       si != sortedSampleTag.end(); ++si)
  {
    // remove the Sensor ID from the short ID to get the label
    arcfg.label = (*si)->getShortId() - getId();

    // round down the floating point rates
    arcfg.rate  = (short) floor( (*si)->getRate() );

    // Note - ARINC samples have only one variable...
    const Variable* var = (*si)->getVariables().front();

    err("labl: %04o  rate: %2d  (float)rate: %6.3f  units: %8s  name: %20s  longname: %s",
        arcfg.label, arcfg.rate, (*si)->getRate(),
        (var->getUnits()).c_str(), (var->getName()).c_str(), (var->getLongName()).c_str());

    ioctl(ARINC_SET, &arcfg, sizeof(arcfg_t));
  }
  sortedSampleTag.clear();
  ioctl(ARINC_MEASURE,(const void*)0,0);
  ioctl(ARINC_GO,(const void*)0,0);

  RTL_DSMSensor::open(flags);
}

void DSMArincSensor::close() throw(atdUtil::IOException)
{
  err("");
  ioctl(ARINC_RESET, (const void*)0,0);
  RTL_DSMSensor::close();
}

/**
 * Since each sample contains it's own time tag then the block sample's time tag
 * (obtained from samp->getTimeTag()) is useless.
 */
bool DSMArincSensor::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
  const tt_data_t *pSamp = (const tt_data_t*) samp->getConstVoidDataPtr();
  int nfields = samp->getDataByteLength() / sizeof(tt_data_t);

  for (int i=0; i<nfields; i++) {

//     if (i == nfields-1)
//       err("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
//           (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned long)0xffffff00) );

    SampleT<float>* outs = getSample<float>(1);
    outs->setTimeTag(pSamp[i].time);

    unsigned short label = pSamp[i].data && 0xff;

    // set the sample id to sum of sensor id and label
    outs->setShortId(getId() + label);
    outs->setDataLength(1);
    float* d = outs->getDataPtr();

    d[0] = processLabel(pSamp[i].data);
    results.push_back(outs);
  }

  return true;
}

void DSMArincSensor::fromDOMElement(const DOMElement* node)
  throw(atdUtil::InvalidParameterException)
{
  RTL_DSMSensor::fromDOMElement(node);
  XDOMElement xnode(node);

  // parse attributes...
  if(node->hasAttributes()) {

    // get all the attributes of the node
    DOMNamedNodeMap *pAttributes = node->getAttributes();
    int nSize = pAttributes->getLength();

    for(int i=0;i<nSize;++i) {
      XDOMAttr attr((DOMAttr*) pAttributes->item(i));

      // get attribute name
      const string& aname = attr.getName();
      const string& aval = attr.getValue();

      if (!aname.compare("sim_xmit"))
        sim_xmit = !aval.compare("true");
      err("sim_xmit = %d", sim_xmit);
    }
  }
//   // parse elements...
//   DOMNode* child;
//   for (child = node->getFirstChild(); child != 0;
//        child = child->getNextSibling())
//   {
//     if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
//     XDOMElement xchild((DOMElement*) child);
//     const string& elname = xchild.getNodeName();
//
//     if (!elname.compare("arcfg")) {
//
//       if ( xchild.getAttributeValue("label").c_str()[0] != '0' )
//         throw atdUtil::InvalidParameterException (__PRETTY_FUNCTION__,
//           "not an octal label:", xchild.getAttributeValue("label").c_str());
//
//       Arcfg element;
//       element.label = strtoul(xchild.getAttributeValue("label").c_str(),NULL,0);
//       element.rate  = strtoul(xchild.getAttributeValue("rate").c_str(),NULL,0);
//       element.desc  = xchild.getAttributeValue("desc").c_str();
//
// #ifdef XML_DEBUG
// //       err("labl: %04o  rate: %2d  desc: %s", element.label, element.rate, element.desc.c_str());
//       printf("labl: %04o  rate: %2d  desc: %s\n", element.label, element.rate, element.desc.c_str());
// #endif
//       set<int>::iterator li = labels.find(element.label);
//       if (li != labels.end())
//         throw atdUtil::InvalidParameterException (__PRETTY_FUNCTION__,
//           "duplicate label configured", xchild.getAttributeValue("label").c_str());
//
// //       pair<set<int>::iterator, bool > result = labels.insert(element.label);
// //       if ( ! result.second )
// //         throw atdUtil::InvalidParameterException (__PRETTY_FUNCTION__,
// //           "duplicate label configured", xchild.getAttributeValue("label").c_str());
//
//       arcfgs.insert( element );
//     }
//   }
}
