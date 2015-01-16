// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
 Copyright 2005 UCAR, NCAR, All Rights Reserved

 $Revision$

 $LastChangedDate$

 $LastChangedRevision$

 $LastChangedBy$

 $HeadURL$

 ******************************************************************
 */


#include <nidas/dynld/raf/DSMMesaSensor.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,DSMMesaSensor)

DSMMesaSensor::DSMMesaSensor(): radar_info(),p260x_info(),counter_info()
{
    ILOG(("constructor"));

    memset(&radar_info, 0, sizeof(radar_info));
    memset(&counter_info, 0, sizeof(counter_info));
    memset(&p260x_info, 0, sizeof(p260x_info));
}

DSMMesaSensor::~DSMMesaSensor()
{
}

IODevice* DSMMesaSensor::buildIODevice() throw(n_u::IOException)
{
    setDriverTimeTagUsecs(USECS_PER_MSEC);
    return new UnixIODevice();
}

SampleScanner* DSMMesaSensor::buildSampleScanner()
throw(n_u::InvalidParameterException)
{
    return new DriverSampleScanner();
}


/*---------------------------------------------------------------------------*/
void DSMMesaSensor::open(int flags) throw(n_u::IOException,
        n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    ILOG(("open-begin"));

    sendFPGACodeToDriver();

    // Send down rates.
    if (counter_info.rate > 0)
        ioctl(COUNTERS_SET, &counter_info, sizeof(counter_info));

    if (p260x_info.rate > 0)
        ioctl(PMS260X_SET, &p260x_info, sizeof(p260x_info));

    if (radar_info.rate > 0)
        ioctl(RADAR_SET, &radar_info, sizeof(radar_info));

    ILOG(("open-end"));
}

/*---------------------------------------------------------------------------*/
bool DSMMesaSensor::process(const Sample * samp, list<const Sample *>& results)
throw()
{
    // pointer to 16 bit raw data
    const unsigned short * sp =
        (const unsigned short *)samp->getConstVoidDataPtr();

    // We send up the SampleID as part of the data packet.
    unsigned int nvalues = samp->getDataByteLength() / sizeof(short);

    short sampleID = *sp++;
    nvalues--;

    SampleT<float>* osamp = getSample<float>(nvalues);
    osamp->setTimeTag(samp->getTimeTag() - getLagUsecs());
    osamp->setId(sampleID + getId());

    float * dptr = osamp->getDataPtr();

    if (sampleID == ID_260X)
    {
        // these values must correspond to the sequence of
        // <variable> tags in the <sample> for this sensor.
        if (nvalues > 1) {
            *dptr++ = *sp++;	// Strobes.
            *dptr++ = *sp++;	// Resets.
            nvalues -= 2;
        }

#ifdef HOUSE_260X
        for (size_t iout = 0; iout < 8 && nvalues; ++iout,nvalues--)
            *dptr++ = *sp++;
#endif

        for (size_t iout = 0; iout < TWO_SIXTY_BINS && nvalues; ++iout,nvalues--)
            *dptr++ = *sp++;
    }
    else
    {
        unsigned int nvariables = 0;

        switch (sampleID)
        {
        case ID_COUNTERS:
            nvariables = counter_info.nChannels;
            break;
        case ID_RADAR:
            nvariables = radar_info.nChannels;
            break;
        case ID_DIG_IN:
            break;
        case ID_DIG_OUT:
            break;
        }

        if (nvalues != nvariables)
        {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "Mesa sample id %d (dsm=%d,sensor=%d): Expected %d raw values, got %d",
                    samp->getId(),GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()),
                    nvariables, nvalues);
            return false;
        }


        if (sampleID == ID_RADAR)
        {
            unsigned short value;

            for (size_t i = 0; i < nvalues; ++i)
            {
                value = *sp++;
                // Radar altimeter returns 65536 when sitting on ground, remove it.
                if (value > 60000 || value == 0) *dptr++ = floatNAN;
                else *dptr++ = (float)value;
            }
        }
        else
        {
            for (size_t i = 0; i < nvalues; ++i)
                *dptr++ = *sp++;
        }
    }
    results.push_back(osamp);
    return true;
}

/*---------------------------------------------------------------------------*/
void DSMMesaSensor::fromDOMElement(const xercesc::DOMElement * node)
throw(n_u::InvalidParameterException)
{
    DSMSensor::fromDOMElement(node);

    int rate, i;
    dsm_sample_id_t sampleId;

    for (SampleTagIterator ti = getSampleTagIterator(); ti.hasNext(); )
    {
        const SampleTag* tag = ti.next();
        rate = irigClockRateToEnum((int)tag->getRate());
        sampleId = tag->getId();

        // ILOG(("fromDOMElement, dsm id=" << tag->getDSMId() << ", sensor id=" << tag->getSensorId() << ", sample id=" << tag->getSampleId() << ", rate=" << tag->getRate()));

        switch (tag->getSampleId())
        {
        case ID_COUNTERS:
            ILOG(("DSMMesaSensor::fromDOMElement() ID_COUNTERS selected."));
            i = 0;
            for (	VariableIterator vi = tag->getVariableIterator();
                    i < N_COUNTERS && vi.hasNext();
                    ++i, vi.next())
            {
                ++counter_info.nChannels;
                counter_info.rate = (int)tag->getRate();
                if (counter_info.rate != 100)
                    throw n_u::InvalidParameterException(getName(), "Counter",
                            "Sample rate must be 100.");
            }
            break;
        case ID_DIG_IN:
            ILOG(("DSMMesaSensor::fromDOMElement() DIG_IN not implemented yet."));
            break;
        case ID_DIG_OUT:
            ILOG(("DSMMesaSensor::fromDOMElement() DIG_OUT not implemented yet."));
            break;
        case ID_260X:
            ILOG(("DSMMesaSensor::fromDOMElement() ID_260X selected."));
            p260x_info.nChannels = 1;
            p260x_info.rate = (int)tag->getRate();
            break;
        case ID_RADAR:
            ILOG(("DSMMesaSensor::fromDOMElement() ID_RADAR selected."));
            radar_info.nChannels = 1;
            radar_info.rate = (int)tag->getRate();
            break;
        default:
            PLOG(("DSMMesaSensor::fromDOMElement() Unknown sampleID ") << tag->getSampleId() );
        }

        /*
           for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); )
           {
           const Variable* var = vi.next();
           ILOG(("var=") << var->getName() );
           }
           */
    }

    ILOG(("fromDOMElement-end"));
}

/*---------------------------------------------------------------------------*/
void DSMMesaSensor::sendFPGACodeToDriver() throw(n_u::IOException)
{
    string fname("/usr/local/firmware/mesa_fpga_file.bit");
    const Parameter* pparm = getParameter("firmware");
    if (pparm && pparm->getType() == Parameter::STRING_PARAM &&
            pparm->getLength() == 1)
        fname= pparm->getStringValue(0);

    FILE * fp;
    int ilen = 0;

    // Open up the FPGA program from disk...
    ILOG(("opening ") << fname);
    if ((fp = fopen(fname.c_str(), "rb")) == NULL)
        throw n_u::IOException(fname,"open",errno);

    try {
        // Send the Load FPGA Program ioctl
        DLOG(("go select file type"));
        selectfiletype(fp,fname);
        DLOG(("file type selected"));

        ioctl(MESA_LOAD_START,0,0);

        struct mesa_prog prog;
        do
        {
            prog.len = fread(prog.buffer, 1, MESA_MAX_FPGA_BUFFER, fp);
            if (ferror(fp)) throw n_u::IOException(fname,"read",errno);
            if (prog.len > 0) ioctl(MESA_LOAD_BLOCK, &prog,
                    sizeof(struct mesa_prog));
            ilen += prog.len;
        }
        while( !feof(fp) );

        ioctl(MESA_LOAD_DONE, 0, 0);
        fclose(fp);
    }
    catch (const n_u::IOException& e) {
        fclose(fp);
        throw e;
    }
    ILOG(("%s: Done sending %s file down, len=%d",
                getName().c_str(),fname.c_str(),ilen));
}

/*---------------------------------------------------------------------------*/
void DSMMesaSensor::selectfiletype(FILE * fp,const string& fname)
throw(n_u::IOException)
{
    unsigned char b[14];

    /* Read in the interesting parts of the file header. */
    if (fread(b, 1, sizeof(b), fp)  != sizeof(b)) {
        if (ferror(fp)) throw n_u::IOException(fname,"read",errno);
        throw n_u::EOFException(fname,"read");
    }

    /* Figure out what kind of file we have. */
    if ((b[0] == 0x00) && (b[1] == 0x09) && (b[11] == 0x00) &&
            (b[12] == 0x01) && (b[13] == 'a'))
    {       /* Looks like a .BIT file. */

        if (fread(b, 1, 4, fp)  != 4) {
            if (ferror(fp)) throw n_u::IOException(fname,"read",errno);
            throw n_u::EOFException(fname,"read");
        }
        // Number of bytes in device image portion of file.
        long ImageLen = (((unsigned long)b[0] << 24) |
                ((unsigned long)b[1] << 16) |
                ((unsigned long)b[2] << 8) |
                (unsigned long)b[3]) ;
        ILOG(("%s: is a .BIT file, configuration length = %d bytes",
                    fname.c_str(),ImageLen));
        /* We leave the file position set to the next byte in the file,
           which should be the first byte of the body of the data image. */
    }
    else if ((b[0] == 0xFF) && (b[4] == 0x55) && (b[5] == 0x99) &&
            (b[6] == 0xAA) && (b[7] == 0x66)) {
        /* Looks like a PROM file. */
        ILOG(("%s: Looks like a PROM file.",fname.c_str()));
    }
    else {       /* It isn't something we know about. */
        throw n_u::IOException(fname,"read","unknown file type");
    }
}
