#define DEBUG0
#define PRINTINT	100
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision: $

    $LastChangedBy: $

    $HeadURL: $

 ********************************************************************

*/
#include <AnalogSensorFilter.h>
#include <DSMAnalogSensor.h>
#include <Sample.h>
#include <SamplePool.h>
#include <SampleClient.h>
#include <a2d_driver.h>
#include <iostream>

using namespace dsm;
using namespace std;


AnalogSensorFilter::AnalogSensorFilter():  rates(0)
{

	xfrctr = 1;	// Start at xfer 0
	Ictr = 1;
}
    
AnalogSensorFilter::~AnalogSensorFilter()
{
    delete [] rates;
	free(sumbuf);
}

void AnalogSensorFilter::setAnalogSensor(DSMAnalogSensor* sensor)
{
    const std::vector<int>& channels = sensor->getChannels();

cerr << "in AnalogSensorFilter::setAnalogSensor" << endl;
    rates = new int[channels.size()];
	outsize = 0;	
    for (int i = 0; i < (int)channels.size(); i++)
	{
        rates[i] = sensor->getSamplingRate(channels[i]);
		if(rates[i] > 0)norm[i] = (float)rates[i]/(float)A2D_MAX_RATE;
		else norm[i] = 1.0;
		outsize += rates[i];
	}
	initPtrs();
	sumbuf = (long *)malloc(outsize);
	memset(sumbuf, 0, outsize*sizeof(short));
}

bool AnalogSensorFilter::receive(const dsm::Sample* samp)
	throw(atdUtil::IOException,dsm::SampleParseException)
{
	// This is the 100 Hz loop
	for(int i = 0; i < RATERATIO; i++)
	{
		// Loop through all a/d's
		for(int j = 0; j < MAXA2DS; j++)
		{
			// If this A/D channel is active (rates[j] > 0)
			if(rates[j] > 0)
			{
			//Add on the latest sample for this channel
			sumbuf[a2dptr[j]] += 
				((const short *)samp->getConstVoidDataPtr())[i*MAXA2DS+j];
			
				// If at end of cycle for this channel,
				if(Ictr%a2dctr[j] == 0)
				{
					// bump the pointer.
					a2dptr[j] += 1;
				}
			}
		}
		// Increment the sample counter
		Ictr++;
	}

	// If at end of 1 second cycle
	if(xfrctr==INTRP_RATE)
	{
		// Reset Ictr, and xfrctr
		Ictr = 0;			
		xfrctr = 0;

		// Reinitialize pointers
		initPtrs();

		// Normalize data in buffer
		for(int j = 0; j < MAXA2DS; j++)
		{
			for(int k = 0; k < rates[j]; k++)
			{
			if(sumbuf[a2dptr[j]+k] > 0)	
				sumbuf[a2dptr[j]+k] = 
					(long)(0.5 + (float)(sumbuf[a2dptr[j]+k])*norm[j]);
			if(sumbuf[a2dptr[j]+k] < 0)	
				sumbuf[a2dptr[j]+k] = 
					(long)(-0.5 + (float)(sumbuf[a2dptr[j]+k])*norm[j]);
			}
		}

		// Create the output sample	
    	ShortIntSample* outs =
    		SamplePool<ShortIntSample>::getInstance()->getSample(outsize);

//TODO Make sure timetag is obtained from the correct sample. 

		outs->setTimeTag(samp->getTimeTag());
		outs->setId(samp->getId());
		outs->setDataLength(outsize);

//TODO Make certain this data transfer is the correct length
//	Can we use memcpy? Should I use p/p buffers again? 
//	Might be able to make buffer reset more elegant by doing non-add in 
//		first iteration.

    	for (unsigned int i = 0; i < outsize; i++)
		{
			outs->getDataPtr()[i] = (short)(sumbuf[i] & 0xFFFF);
			sumbuf[i] = 0;
		}

#ifdef DEBUG0
		printf("time %08d: datalength %05d: outs = 0x%08X: ", 
				outs->getTimeTag(), 
				outs->getDataLength(), 
				outs);
		printf("\n");
#endif

		distribute(outs);
	}

	xfrctr++;		// Bump the transfer counter

	return true;
}

void AnalogSensorFilter::initPtrs(void)
{
	a2dptr[0] = 0;

	for(int i = 1; i < MAXA2DS; i++)
	{
		a2dptr[i] = a2dptr[i-1] + rates[i-1];
	}	

	for(int i = 0; i < MAXA2DS; i++)
	{
		if(rates[i] !=0)a2dctr[i] = A2D_MAX_RATE/rates[i];
		else a2dctr[i] = 1;
	}
}
