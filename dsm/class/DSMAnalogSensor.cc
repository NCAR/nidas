/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision: $

    $LastChangedBy: $

    $HeadURL:$

 ******************************************************************
*/

#define STOP	3

#include <DSMAnalogSensor.h>
#include <RTL_DevIoctlStore.h>
#include <asm/ioctls.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>

#include <a2d_driver.h>

using namespace dsm;
using namespace std;

DSMAnalogSensor::DSMAnalogSensor(const string& nameArg) :
    RTL_DSMSensor(nameArg)
{

}

DSMAnalogSensor::~DSMAnalogSensor() {
    try {
	close();
    }
    catch(atdUtil::IOException& ioe) {
      cerr << ioe.what() << endl;
    }
}
void DSMAnalogSensor::addChannel(int chan, int rate, int gain,
	int offset)
{
    pair<int,struct chan_info> info;
    info.first = chan;
    info.second.rate = rate;
    info.second.gain = gain;
    info.second.offset = offset;

    chanInfo.insert(info);
    // add channel if it doesn't exist
    if (find(channels.begin(),channels.end(),chan) ==
    	channels.end()) channels.push_back(chan);
}
void DSMAnalogSensor::open(int flags) throw(atdUtil::IOException)
{
  
	RTL_DSMSensor::open(flags);

	A2D_SET a2d;

	for(int chan = 0; chan < MAXA2DS; chan++)
	{
		a2d.Hz[chan] = getSamplingRate(chan);
		a2d.gain[chan] = getGain(chan);
		a2d.offset[chan] = getOffset(chan);
		a2d.status[chan] = 0; 
		a2d.calset[chan] = 0;

		if(chan == 0)a2d.ptr[chan] = 0;
		else 
		{
			a2d.ptr[chan] = a2d.ptr[chan-1] + a2d.Hz[chan-1];
		}
		a2d.ctr[chan] = 0;	// Reset counters	
		if(a2d.Hz[chan] != 0)a2d.norm[chan] = 
					(float)a2d.Hz[chan]/float(A2D_MAX_RATE);
	} 

	FILE *fp;

	if((fp = fopen("filtercoeff.bin", "rb")) == NULL)
	{
		printf("*********No filter coefficients.************\n");
		exit(1);
	}
	fread(&a2d.filter[0], 2, 2048, fp);
	fclose(fp);

	ioctl(A2D_SET_IOCTL, &a2d, sizeof(A2D_SET));
}

void DSMAnalogSensor::close() throw(atdUtil::IOException)
{
    	int msg = STOP;
 	ioctl(A2D_RUN_IOCTL, &msg, sizeof(int));	
	RTL_DSMSensor::close();
}

void DSMAnalogSensor::run(int runmsg) throw(atdUtil::IOException)
{
    ioctl(A2D_RUN_IOCTL, &runmsg, sizeof(int));
    if(runmsg == STOP)RTL_DSMSensor::close();
}


