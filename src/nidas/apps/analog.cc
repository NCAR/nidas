/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#define	RUN	1
#define STOP	2
#define	RESTART	3

#include	<fcntl.h>
#include	<cstdio>
#include	<cstdlib>
#include	<cstring>
#include 	<unistd.h>
#include	<climits>
#include	<iostream>
#include	<iomanip>
#include	<sys/select.h>

#include	<nidas/rtlinux/ioctl_fifo.h>
#include	<nidas/rtlinux/a2d_driver.h>
#include	<nidas/core/RTL_DSMSensor.h>
#include	<nidas/core/AnalogSensorFilter.h>
#include	<nidas/dynld/raf/DSMAnalogSensor.h>
#include	<nidas/core/PortSelectorTest.h>

void A2D_SETDump(A2D_SET *a);
void A2DPtrInit(A2D_SET *a);
void loada2dstruct(A2D_SET *a);

using namespace nidas::core;
using namespace std;

namespace n_r = nidas::dynld::raf;
namespace n_u = nidas::util;

int main(void)
{
	A2D_SET *a2dtopside;  

    	PortSelectorTest* handler = PortSelectorTest::createInstance();
	    handler->start();

	a2dtopside = (A2D_SET *)malloc(sizeof(A2D_SET));

 	n_r::DSMAnalogSensor* a2dSensor = new n_r::DSMAnalogSensor("/dev/dsma2d0");

	loada2dstruct(a2dtopside);

	A2DPtrInit(a2dtopside);

	for(int i = 0; i < MAXA2DS; i++)
	{
		if(a2dtopside->Hz[i] != 0)
		{
    		a2dSensor->addChannel(
			i,
			a2dtopside->Hz[i],
			a2dtopside->gain[i],
			a2dtopside->offset[i]);
		}
	}

   	try 
	{
   		a2dSensor->open(O_RDONLY);
   		a2dSensor->run(RUN);

   		// add the a2d to the SensorPortHandler
   		handler->addSensorPort(a2dSensor);
	}
     		
    catch (n_u::IOException& ioe) 
	{
   		cerr << ioe.what() << endl;
   		return 1;
   	}

cout << "About to define a2dfilt = new AnalogSensorFilter" << endl;

   	AnalogSensorFilter* a2dfilt = new AnalogSensorFilter();
cout << "About to launch setAnalogSensor" << endl;

   	// tell a2dfilt what it's sensor is, so it can get
   	// the necessary parameters, like rate
   	a2dfilt->setAnalogSensor(a2dSensor);

cout << "About to launch addSampleClient" << endl;
   	// hook them together, so a2dfilt receives the samples from a2d
   	a2dSensor->addSampleClient(a2dfilt);
    try {
	handler->join();
    }
    catch(n_u::Exception& e) {
        cerr << "join exception: " << e.what() << endl;
	return 1;
    }
    delete handler;
    return 0;
}

void A2D_SETDump(A2D_SET *a2d)
{
    int i;
    for(i = 0;i < 8; i++)
   	{
		printf("Gain_%1d= %3d:\t", i, a2d->gain[i]);
		printf("Hz_%1d  = %3d:\t", i, a2d->Hz[i]);
		printf("Ctr_%1d = %3d:\t", i, a2d->ctr[i]);
		printf("Ptr_%1d = 0x%08X\n", i, a2d->ptr[i]);
	}

	printf("Vcalx8  = %3d\n", a2d->vcalx8);
	printf("filter0 = 0x%04X\n", a2d->filter[0]);

  	return; 
}

void A2DPtrInit(A2D_SET *a2d)
{
        int i;

        a2d->ptr[0] = 0;

        for(i = 1;i < MAXA2DS; i++)
        {
                a2d->ptr[i] = a2d->ptr[i-1] + a2d->Hz[i-1];
        }
return;
}

void loada2dstruct(A2D_SET *a2d)
{
        int i;
        FILE *fp;

/*
Load up some phoney data in the A2D control structure, A2D_SET
*/
        int freqs[8] = {1, 2, 5, 10, 25, 50, 100, 100};
        for(i = 0; i < MAXA2DS; i++)
        {
                a2d->gain[i] = i;
                a2d->Hz[i] = freqs[i];
                if(a2d->Hz[i] != 0)a2d->ctr[i] = A2D_MAX_RATE/a2d->Hz[i];
                else a2d->ctr[i] = 1;
                a2d->status[i] = 0;
                if(a2d->Hz[i] != 0)a2d->norm[i] =
                        (float)a2d->Hz[i]/(float)A2D_MAX_RATE;
                else a2d->norm[i] = 1.0;
        	a2d->calset[i] = 0;
        	a2d->offset[i] = 0;

        }
        a2d->vcalx8 = 128;

        if((fp = fopen("filtercoeff.bin", "rb")) == NULL)
                {
                printf("No filter file!\n");
                exit(1);
                }
        fread(&a2d->filter[0], 2, 2048, fp);

#ifdef DEBUG
        for(i = 0; i < 10; i++)
                {
                printf("0x%04X\n", a2d->filter[i]);
                }
#endif

        return;
}
