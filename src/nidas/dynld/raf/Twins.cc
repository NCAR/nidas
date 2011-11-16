// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
    _badRawSamples(0),
    _waveSize(0),
    _waveRate(-1.0),
    _outputChannel(0)
{
    setLatency(0.1);
}

Twins::~Twins()
{
}

void Twins::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    A2DSensor::open(flags);

    // Configure the desired waveform rate in Hz
    // (how many complete waveforms to send out per second).
    struct D2A_Config d2acfg;
    d2acfg.waveformRate = (int)rint(_waveRate);
    ioctl(DMMAT_D2A_SET_CONFIG, &d2acfg, sizeof(d2acfg));

    // Get the conversion factors for D2A, use them to generate the waveforms
    struct DMMAT_D2A_Conversion conv;
    ioctl(DMMAT_D2A_GET_CONVERSION,&conv,sizeof(conv));

    // create the output waveform, send to device
    D2A_WaveformWrapper wave(_outputChannel,_waveSize);
    createRamp(conv,wave);
    ioctl(DMMAT_ADD_WAVEFORM, wave.c_ptr(),wave.c_size());

    // Get the actual number of input channels on the card.
    // This depends on differential/single-ended jumpering
    int nchan;

    ioctl(NIDAS_A2D_GET_NCHAN,&nchan,sizeof(nchan));

    struct nidas_a2d_config cfg;
    cfg.scanRate = (int)rint(_waveRate);
    cfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10;

    ioctl(NIDAS_A2D_SET_CONFIG, &cfg, sizeof(cfg));

    for(unsigned int i = 0; i < _sampleCfgs.size(); i++) {
        struct nidas_a2d_sample_config& scfg = _sampleCfgs[i].cfg();
    
        for (int j = 0; j < scfg.nvars; j++) {
            if (scfg.channels[j] >= nchan) {
                ostringstream ost;
                ost << "channel number " << scfg.channels[j] <<
                    " is out of range, max=" << nchan;
                throw n_u::InvalidParameterException(getName(),
                    "channel",ost.str());
            }
        }
#ifdef DEBUG
        cerr << "sindex=" << scfg.sindex << " nvars=" << scfg.nvars << 
            " rate=" << scfg.rate << " filterType=" << scfg.filterType <<
            " nFilterData=" << scfg.nFilterData << endl;
#endif
        ioctl(NIDAS_A2D_CONFIG_SAMPLE, &scfg,
            sizeof(struct nidas_a2d_sample_config)+scfg.nFilterData);
    }

    ioctl(DMMAT_START,0,0);
}


void Twins::close() throw(n_u::IOException)
{
    ioctl(DMMAT_STOP,0,0);
    DSC_A2DSensor::close();
}

void Twins::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSC_A2DSensor::fromDOMElement(node);

    // get the output D2A channel number
    const Parameter *p = getParameter("outputChannel");
    if (!p) 
      throw n_u::InvalidParameterException(getName(), "outputChannel", "not found");
    if (p->getLength() != 1) 
      throw n_u::InvalidParameterException(getName(), "outputChannel", "not of length 1");
    _outputChannel = (int)p->getNumericValue(0);

    /*
       We need to be sure that all samples have the same rate and that all
       variables of length > 1 (i.e. the wave forms) have the same length 
       */
    if (_sampleInfos.size() == 0)
        throw n_u::InvalidParameterException(getName(), "samples",
                "No _sampleInfos - can't validate samples/variables");

    for (unsigned int i=0; i<_sampleInfos.size(); i++) {
        A2DSampleInfo& sinfo = _sampleInfos[i];
        const SampleTag* stag = sinfo.stag;
        const vector<const Variable*>& vars = stag->getVariables();

        if (_waveRate < 0.0) _waveRate = stag->getRate();
        else if (stag->getRate() != _waveRate)
            throw n_u::InvalidParameterException(getName(), "sample rates",
                    "All Sample Rates need to be the same for this sensor.");

        for (int j = 0; j < sinfo.nvars; j++) {
            const Variable* var = vars[j];
            if (var->getLength() > 1) {
                if (_waveSize <= 0) {
                    _waveSize = var->getLength();
                } else {
                    if ((int)var->getLength() != _waveSize) 
                        throw n_u::InvalidParameterException(getName(), "Var length",
                                "All >1 length vars must be the same length for this sensor.");
                }
            }
        }
    }
}

void Twins::createRamp(const struct DMMAT_D2A_Conversion& conv,D2A_WaveformWrapper& wave)
{
    D2A_Waveform* wp = wave.c_ptr();
    int chan = wp->channel;

    // scaling from volts to D2A counts for the output channel.
    float d2ascale = (conv.cmax[chan] - conv.cmin[chan]) / (conv.vmax[chan] - conv.vmin[chan]);

    float istart = 35.00;   // starting current, mA
    float irange = 50.00;   // current range

    // for 20 ohm sense resistor
    float VIconvert = 10.0 / 125.0;             // volts per mA

    float Vstart = istart * VIconvert;          // eg. 40 mA = 3.2V, 80 mA = 6.4V
    float Vend = Vstart + (irange * VIconvert); // eg. 40 mA + 35 mA = 3.2V+2.8V = 6.0V
    float Vstep = (Vend - Vstart) / (float)(wp->size - 1);  // Voltage steps for ramp

    // Generate full laser scan ramp over wave size points
    for (int i=0; i < wp->size; i++ )
    {
        float v;
        if (i <= 29) // Ramp starts out as flat
            v = 0.0;
        else // Increment laser scan voltage and convert to counts
            v = Vstart + i * Vstep;

        wp->point[i] = (int)rint((v - conv.vmin[chan]) * d2ascale) + conv.cmin[chan];
    }
}

