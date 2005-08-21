/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <arinc.h>
#include <DSMArincSensor.h>
#include <RTL_DevIoctlStore.h>

#include <math.h>
#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <set>

using namespace std;
using namespace dsm;
using namespace xercesc;

//CREATOR_ENTRY_POINT(DSMArincSensor);

DSMArincSensor::DSMArincSensor() :
  _nanf(nanf("")), _speed(AR_HIGH), _parity(AR_ODD), sim_xmit(false)
{
}

DSMArincSensor::~DSMArincSensor() {
}

void DSMArincSensor::open(int flags) throw(atdUtil::IOException)
{
  err("");

  ioctl(ARINC_RESET, (const void*)0,0);

  // sort SampleTags by rate then by label
  set <const SampleTag*, SortByRateThenLabel> sortedSampleTags
    ( getSampleTags().begin(), getSampleTags().end() );

  if (sim_xmit) {
    err(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> transmitting");
    ioctl(ARINC_SIM_XMIT,(const void*)0,0);
  }
  else
    err("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< receiving");

  arcfg_t arcfg;
  for (set<const SampleTag*>::const_iterator si = sortedSampleTags.begin();
       si != sortedSampleTags.end(); ++si)
  {
    // remove the Sensor ID from the short ID to get the label
    arcfg.label = (*si)->getSampleId();

    // round down the floating point rates
    arcfg.rate  = (short) floor( (*si)->getRate() );

    // Note - ARINC samples have only one variable...
    const Variable* var = (*si)->getVariables().front();

    // establish a list of which samples are processed.
    _processed[arcfg.label] = (*si)->isProcessed();

    err("proc: %s labl: %04o  rate: %2d %6.3f  units: %8s  name: %20s  longname: %s",
        _processed[arcfg.label]?"Y":"N", arcfg.label, arcfg.rate, (*si)->getRate(),
        (var->getUnits()).c_str(), (var->getName()).c_str(), (var->getLongName()).c_str());

    ioctl(ARINC_SET, &arcfg, sizeof(arcfg_t));
  }
  sortedSampleTags.clear();
  ioctl(ARINC_MEASURE,(const void*)0,0);

  archn_t archn;
  archn.speed  = _speed;
  archn.parity = _parity;
  ioctl(ARINC_GO, &archn, sizeof(archn_t));

  RTL_DSMSensor::open(flags);
}

void DSMArincSensor::close() throw(atdUtil::IOException)
{
  err("");
  ioctl(ARINC_RESET, (const void*)0,0);
  RTL_DSMSensor::close();
}

/**
 * Each label contains it's own time tag in seconds
 * since 00:00 GMT. The input sample's time tag is
 * used to set the date portion of the output sample timetags.
 */
bool DSMArincSensor::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
  const tt_data_t *pSamp = (const tt_data_t*) samp->getConstVoidDataPtr();
  int nfields = samp->getDataByteLength() / sizeof(tt_data_t);

  // absolute time at 00:00 GMT of day.
  dsm_time_t t0day = samp->getTimeTag() -
  	(samp->getTimeTag() % USECS_PER_DAY);
  dsm_time_t tt;

  for (int i=0; i<nfields; i++) {

//     if (i == nfields-1)
//       err("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
//           (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned long)0xffffff00) );

    unsigned short label = pSamp[i].data & 0xff;
//     err("%3d/%3d %08x %04o", i, nfields, pSamp[i].data, label );
    if (!_processed[label]) continue;

    SampleT<float>* outs = getSample<float>(1);

    // pSamp[i].time is the number of milliseconds since midnight
    // for the individual label. Use it to create a correct
    // time tag for the label, which is in units of microseconds.
    tt = t0day + (dsm_time_t)pSamp[i].time * USECS_PER_MSEC;

    // correct for problems around midnight rollover
    if (::llabs(tt - samp->getTimeTag()) > USECS_PER_HALF_DAY) {
        if (tt > samp->getTimeTag()) tt -= USECS_PER_DAY;
        else tt += USECS_PER_DAY;
    }
    outs->setTimeTag(tt);

    // set the sample id to sum of sensor id and label
    outs->setId( getId() + label );
    outs->setDataLength(1);
    float* d = outs->getDataPtr();

    d[0] = processLabel(pSamp[i].data);
    results.push_back(outs);
  }

  return true;
}

void DSMArincSensor::printStatus(std::ostream& ostr) throw()
{
  DSMSensor::printStatus(ostr);

  dsm_arinc_status stat;
  try {
    ioctl(ARINC_STAT,&stat,sizeof(stat));

    ostr << "<td>" <<
      "lps="         << stat.lps_cnt <<
      "/"            << stat.lps <<
      ", poll="      << stat.poll << "Hz" <<
      ", overflow="  << stat.overflow <<
      ", underflow=" << stat.underflow <<
      "</td>" << endl;
  }
  catch(const atdUtil::IOException& ioe) {
    ostr << "<td>" << ioe.what() << "</td>" << endl;
  }
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

      if (!aname.compare("speed")) {
        err("%s = %s", aname.c_str(), aval.c_str());
        if (!aval.compare("high"))     _speed = AR_HIGH;
        else if (!aval.compare("low")) _speed = AR_LOW;
        else throw atdUtil::InvalidParameterException
               (DSMSensor::getName(),aname,aval);
      }
      else if (!aname.compare("parity")) {
        err("%s = %s", aname.c_str(), aval.c_str());
        if (!aval.compare("odd"))       _parity = AR_ODD;
        else if (!aval.compare("even")) _parity = AR_EVEN;
        else throw atdUtil::InvalidParameterException
               (DSMSensor::getName(),aname,aval);
      }
      else if (!aname.compare("sim_xmit"))
        sim_xmit = !aval.compare("true");
    }
  }
}
