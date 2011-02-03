/*
 ******************************************************************
    Copyright 2010 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: January 2, 2011 $

    $LastChangedRevision: $

    $LastChangedBy: ryano $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/Twins.cc $

 ******************************************************************
*/

#include <nidas/dynld/raf/Twins.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Variable.h>
#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(Twins)

Twins::Twins() :
    DSC_A2DSensor(),
    _badRawSamples(0)
{
    setLatency(0.1);
}

Twins::~Twins()
{
    if (_ramp) free(_ramp);
}

//bool Twins::isRTLinux() const
//{
    //const string& dname = getDeviceName();
    //string::size_type fs = dname.rfind('/');
    //if (fs != string::npos && (fs + 10) < dname.length() &&
        //dname.substr(fs+1,10) == "rtldsc_a2d")
                //rtlinux = 1;
    ////else rtlinux = 0;

    ////return rtlinux == 1;
//}

IODevice* Twins::buildIODevice() throw(n_u::IOException)
{
    //if (isRTLinux()) return new RTL_IODevice();
    //else return new UnixIODevice();
    return new UnixIODevice();
}

SampleScanner* Twins::buildSampleScanner()
    	throw(nidas::util::InvalidParameterException)
{
    return new DriverSampleScanner();
}

void Twins::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    DSC_A2DSensor::open(flags);

    createRamp();

    // Configure the desired waveform rate
    // in Hertz (how many complete waveforms to send out per second).
    // All waveforms are output at the same rate.
    struct D2A_Config d2acfg;
    d2acfg.waveformRate = 50;
    ioctl(DMMAT_D2A_SET_CONFIG, &d2acfg, sizeof(d2acfg));

    int outputChannel = 0;
    D2A_WaveformWrapper wave(outputChannel,_waveSize);
    D2A_Waveform *wp = wave.c_ptr();

    memcpy(&wp->point, _ramp, sizeof(int)*_waveSize);

    // How to send a waveform
    //printf("Sending wave (i*7)\n");
    n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: Sending wave ",getName().c_str());
    ioctl(DMMAT_ADD_WAVEFORM, wp, sizeof(*wp));

    // Get the actual number of input channels on the card.
    // This depends on differential/single-ended jumpering
    int nchan;

    ioctl(NIDAS_A2D_GET_NCHAN,&nchan,sizeof(nchan));

    struct nidas_a2d_config cfg;
    cfg.scanRate = getScanRate();
    cfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10;

    ioctl(NIDAS_A2D_SET_CONFIG, &cfg, sizeof(cfg));

    for(unsigned int i = 0; i < _sampleCfgs.size(); i++) {
        struct nidas_a2d_sample_config* scfg = _sampleCfgs[i];
    
        for (int j = 0; j < scfg->nvars; j++) {
            if (scfg->channels[j] >= nchan) {
                ostringstream ost;
                ost << "channel number " << scfg->channels[j] <<
                    " is out of range, max=" << nchan;
                throw n_u::InvalidParameterException(getName(),
                    "channel",ost.str());
            }
        }
        ioctl(NIDAS_A2D_CONFIG_SAMPLE, scfg,
            sizeof(struct nidas_a2d_sample_config)+scfg->nFilterData);
    }

    ioctl(DMMAT_START,0,0);
}


void Twins::close() throw(n_u::IOException)
{
    ioctl(DMMAT_STOP,0,0);
    DSC_A2DSensor::close();
}

bool Twins::process(const Sample* insamp, std::list<const Sample*>& results) throw()
{
    // pointer to raw A2D counts
    const short* sp = (const short*) insamp->getConstVoidDataPtr();
    int nval = (insamp->getDataByteLength() - sizeof(short))/sizeof(short);

    // raw data are shorts
    if (insamp->getDataByteLength() % sizeof(short)) {
        _badRawSamples++;
        return false;
    }

    // number of short values in this raw sample.
    int nvalues = insamp->getDataByteLength() / sizeof(short);
    if (nvalues < 1 || nvalues > _waveSize+1) {
        _badRawSamples++;
        return false;      // nothin
    }

    // Sample id should be in first short
    short sampId = *sp; sp++;

    // Allocate sample
    SampleT<float> * outs = getSample<float>(_waveSize);
    outs->setTimeTag(insamp->getTimeTag());
    outs->setId(getId() + sampId);

    // extract data
    float *dout = outs->getDataPtr();
    int iout;

    for (iout = 0; iout < std::min(nval, _waveSize); iout++)
        *dout++ = (float)*sp++;
    for ( ; iout < _waveSize; iout++) *dout++ = floatNAN;

    results.push_back(outs);

    return true;
}

void Twins::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

   ostr << "Twins::printStatus() called" << endl;
   return;
/*
 * Appears to be a direct copy of DSC_A2DSensor:: printStatus
 * could be handy later

    struct DMMAT_A2D_Status stat;
    try {
	ioctl(DMMAT_A2D_GET_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "FIFO: over=" << stat.fifoOverflows << 
		", under=" << stat.fifoUnderflows <<
		", not empty=" << stat.fifoNotEmpty <<
		", lostSamples=" << stat.missedSamples <<
		", irqs=" << stat.irqsReceived;
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
*/
}

void Twins::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{
    DSC_A2DSensor::addSampleTag(tag);
}

void Twins::setA2DParameters(int ichan,int gain,int bipolar)
            throw(n_u::InvalidParameterException)
{
    DSC_A2DSensor::setA2DParameters(ichan,gain,bipolar);
}

void Twins::getBasicConversion(int ichan,
    float& intercept, float& slope) const
{
    DSC_A2DSensor::getBasicConversion(ichan, intercept, slope);
}


void Twins::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
  DSC_A2DSensor::fromDOMElement(node);

  /*
     We need to be sure that all samples have the same rate and that all
     variables of length > 1 (i.e. the wave forms) have the same length 
  */
  if (_sampleInfos.size() == 0)
    throw n_u::InvalidParameterException(getName(), "samples",
                  "No _sampleInfos - can't validate samples/variables");

  A2DSampleInfo* sinfo;
  bool foundWaveSize = false;
  float sRate = 0.0;        // used to verify all sample rates are the same
  for (unsigned int i=0; i<_sampleInfos.size(); i++) {
    sinfo = _sampleInfos[i];
    const SampleTag* stag = sinfo->stag;
    const vector<const Variable*>& vars = stag->getVariables();

    if (i==0) sRate = stag->getRate();
    else if (stag->getRate() != sRate)
      throw n_u::InvalidParameterException(getName(), "sample rates",
                "All Sample Rates need to be the same for this sensor.");

    for (int j = 0; j < sinfo->nvars; j++) {
      const Variable* var = vars[j];
      if (var->getLength() > 1) {
        if (!foundWaveSize) {
          _waveSize = var->getLength();
          foundWaveSize = true;
        } else {
          if ((int)var->getLength() != _waveSize) 
            throw n_u::InvalidParameterException(getName(), "Var length",
                      "All >1 length vars must be the same length for this sensor.");
        }
      }
    }
  }
}

void Twins::createRamp()
{
  float Vmin, Vmax, Vconvert, VIconvert, Vstart, Vend, Vstep;

  float istart = 35.00;
  float irange = 50.00;

  // Set laser scan voltage parameters
  Vmin = 0.0;                           // 0-10V scans laser 0-125 mA
                                        // for 20 ohm sense (unipolar mode)
  Vmax = 10.0;
  Vconvert = 4096 / 10.0;               // Counts per volt (12 bit range)
  VIconvert = 10.0 / 125.0;             // 0.08, = volts per mA
                                        // for 20 ohm sense resistor
  Vstart = istart * VIconvert;          // eg. 40 mA = 3.2V, 80 mA = 6.4V
  Vend = Vstart + (irange * VIconvert); // eg. 40 mA + 35 mA = 3.2V+2.8V = 6.0V
  Vstep = (Vend - Vstart) / (float)(_waveSize - 1);  // Voltage steps for ramp

  _ramp = (int *) malloc(sizeof(int)*_waveSize);

  // Generate full laser scan ramp over all _waveSize points
  for (int i=0; i<_waveSize; i++ )
  {
    if (i <= 29) // Ramp starts out as flat
      _ramp[i] = 0;
    else // Increment laser scan voltage and convert to counts
      _ramp[i] = (int)(Vconvert * (Vstart + ((float)i * Vstep)));
  }
}

