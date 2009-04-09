/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/rtlinux/arinc.h>                // TODO there are two arinc.h files now!
#include <nidas/dynld/raf/DSMArincSensor.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/util/Logger.h>

#include <cmath>
#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <set>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

DSMArincSensor::DSMArincSensor() :
  _nanf(nanf("")), _speed(AR_HIGH), _parity(AR_ODD), sim_xmit(false)
{
    for (unsigned int label = 0; label < NLABELS; label++)
        _processed[label] = false;
}

DSMArincSensor::~DSMArincSensor() {
}

IODevice* DSMArincSensor::buildIODevice() throw(n_u::IOException)
{
    setDriverTimeTagUsecs(USECS_PER_MSEC);
    if (DSMEngine::isRTLinux())
        return new RTL_IODevice();
    else
        return new UnixIODevice();
}

SampleScanner* DSMArincSensor::buildSampleScanner()
{
    return new DriverSampleScanner();
}

void DSMArincSensor::open(int flags)
    throw(n_u::IOException, n_u::InvalidParameterException)
{

    DSMSensor::open(flags);

    // Do other sensor initialization.
    init();

    // sort SampleTags by rate then by label
    set <const SampleTag*, SortByRateThenLabel> sortedSampleTags
      ( getSampleTags().begin(), getSampleTags().end() );

    if (sim_xmit) {
      ioctl(ARINC_SIM_XMIT,0,0);
    }

    for (set<const SampleTag*>::const_iterator si = sortedSampleTags.begin();
	 si != sortedSampleTags.end(); ++si)
    {
      arcfg_t arcfg;

      // remove the Sensor ID from the short ID to get the label
      arcfg.label = (*si)->getSampleId();

      // round down the floating point rates
      arcfg.rate  = (short) floor( (*si)->getRate() );

//#define DEBUG
  #ifdef DEBUG
      // Note - ARINC samples have only one variable...
      const Variable* var = (*si)->getVariables().front();

      ILOG(("proc: %s labl: %04o  rate: %2d %6.3f  units: %8s  name: %20s  longname: %s",
	    _processed[arcfg.label]?"Y":"N", arcfg.label, arcfg.rate, (*si)->getRate(),
	    (var->getUnits()).c_str(), (var->getName()).c_str(), (var->getLongName()).c_str()));
  #endif

      ioctl(ARINC_SET, &arcfg, sizeof(arcfg_t));
    }
    sortedSampleTags.clear();
    ioctl(ARINC_MEASURE,0,0);

    archn_t archn;
    archn.speed  = _speed;
    archn.parity = _parity;
    ioctl(ARINC_OPEN, &archn, sizeof(archn_t));
}

void DSMArincSensor::close() throw(n_u::IOException)
{
  ioctl(ARINC_CLOSE,0,0);
  DSMSensor::close();
}

/*
 * Initialize anything needed for process method.
 */
void DSMArincSensor::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    list<const SampleTag*>::const_iterator si;
    for (si = getSampleTags().begin(); si != getSampleTags().end(); ++si) {
	unsigned short label = (*si)->getSampleId();
	// establish a list of which samples are processed.
	_processed[label] = (*si)->isProcessed();
        DLOG(("labl: %04o  processed: %d", label, _processed[label]));
    }
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
//       ILOG(("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
//             (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned long)0xffffff00) ));

    unsigned short label = pSamp[i].data & 0xff;
//     ILOG(("%3d/%3d %08x %04o", i, nfields, pSamp[i].data, label ));
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
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

  dsm_arinc_status stat;
  try {
    ioctl(ARINC_STAT,&stat,sizeof(stat));

    ostr << "<td align=left>" <<
      "lps="         << stat.lps_cnt <<
      "/"            << stat.lps <<
      ", poll="      << stat.pollRate << "Hz" <<
      ", overflow="  << stat.overflow <<
      ", underflow=" << stat.underflow <<
      ", nosync=" << stat.nosync <<
      "</td>" << endl;
  }
  catch(const n_u::IOException& ioe) {
      ostr << "<td>" << ioe.what() << "</td>" << endl;
      n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
  }
}

void DSMArincSensor::fromDOMElement(const xercesc::DOMElement* node)
  throw(n_u::InvalidParameterException)
{
  DSMSensor::fromDOMElement(node);
  XDOMElement xnode(node);

  // parse attributes...
  if(node->hasAttributes()) {

    // get all the attributes of the node
    xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
    int nSize = pAttributes->getLength();

    for(int i=0;i<nSize;++i) {
      XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));

      // get attribute name
      const string& aname = attr.getName();
      const string& aval = attr.getValue();

      if (!aname.compare("speed")) {
        DLOG(("%s = %s", aname.c_str(), aval.c_str()));
        if (!aval.compare("high"))     _speed = AR_HIGH;
        else if (!aval.compare("low")) _speed = AR_LOW;
        else throw n_u::InvalidParameterException
               (DSMSensor::getName(),aname,aval);
      }
      else if (!aname.compare("parity")) {
        DLOG(("%s = %s", aname.c_str(), aval.c_str()));
        if (!aval.compare("odd"))       _parity = AR_ODD;
        else if (!aval.compare("even")) _parity = AR_EVEN;
        else throw n_u::InvalidParameterException
               (DSMSensor::getName(),aname,aval);
      }
      else if (!aname.compare("sim_xmit"))
        sim_xmit = !aval.compare("true");
    }
  }
}
