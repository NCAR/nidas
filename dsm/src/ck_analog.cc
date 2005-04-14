// #define REALTIME
/* analog.cc

   User space code for analog data acquisition

   Original Author: Grant Gray

   Copyright 2005 UCAR, NCAR, All Rights Reserved


   Revisions:

     $LastChangedRevision: 695 $
         $LastChangedDate: 11/3/2004 $
           $LastChangedBy: gray $
                 $HeadURL: $
*/

// A2D_RUN_IOCTL control messages
#define	RUN	0x00000001
#define	STOP	0x00000002
#define	RESTART	0x00000003

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h>
#include <sys/select.h>

#include <ioctl_fifo.h>
#include <a2d_driver.h>
#include <RTL_DSMSensor.h>

using namespace std;

void loada2dstruct(A2D_SET *a);
void A2D_SETDump(A2D_SET *a);

static long *ptr_inbuf, *ptr_outbuf, *ptr_temp;
static int  cyclecount = 100, Cctr;
static int  Ictr = 1, FirstTimeA2D = 1;

int main(int argc, char** argv)
{
  A2D_SET *a2d; 	//set up a control struct 
  A2DSAMPLE buf; 	// and a data struct
  UL run_msg;
  int ii, jj, kk;
  int i, j, k, sumrates = 0, bufsize = 0;
  void *buffer0, *buffer1;

  if(argc > 1)cyclecount = atoi(argv[1]);
  else cyclecount = 1;

  fd_set readfds;

  a2d = (A2D_SET *)malloc(sizeof(A2D_SET));

  cout << "----ANALOG----" << endl; 
  cout << __FILE__ << ": Creating sensor class..." << endl;

  RTL_DSMSensor sensor("/dev/dsma2d0");

  try 
  {
    sensor.open(O_RDONLY);
  }

  catch (atdUtil::IOException& ioe) 
  {
    cerr << ioe.what() << endl;    
    return 1;
  }

  cout << __FILE__ << ": Up Fifo opened" << endl;

  // Load some phoney data into the a2d structure
  a2d = (A2D_SET *)malloc(sizeof(A2D_SET));
  loada2dstruct(a2d);
  A2DPtrInit(a2d);

  for(i = 0; i < MAXA2DS; i++)sumrates += a2d->Hz[i];  // Calculate sum of rates
  bufsize = sumrates*sizeof(long);
  cout << "Sumrates " << sumrates<< ", bufsize " << bufsize << endl;

  if(FirstTimeA2D)
  {   
  	FirstTimeA2D = 0;       // Clear the flag
	// Allocate and initialize ping/pong buffers
	buffer0 = (long *)malloc(bufsize);
	buffer1 = (long *)malloc(bufsize);
	ptr_inbuf = (long *)buffer0;
	ptr_outbuf = (long *)buffer1;

	// Clear both buffers
	memset(buffer0, 0, bufsize);
	memset(buffer1, 0, bufsize);
        cout << "Buffers cleared "<< endl;
  }
  

  cout << __FILE__ << ": Structure loaded" << endl;
  cout << __FILE__ << ": Size of A2D_SET = " << sizeof(A2D_SET) << endl;

  //Send the struct to the a2d_driver
 
  sensor.ioctl(A2D_SET_IOCTL, a2d, sizeof(A2D_SET));	

  cout << __FILE__ << ": Initialization data sent to driver" << endl;

  A2D_SETDump(a2d);

  sensor.ioctl(A2D_CAL_IOCTL, a2d, sizeof(A2D_SET));

  cout << __FILE__ << ": Calibration command sent to driver" << endl;

  run_msg = RUN;	

  sensor.ioctl(A2D_RUN_IOCTL, &run_msg, sizeof(int));
  
  cout << __FILE__ << ": Run command sent to driver" << endl;

  usleep(10000); // Wait for one cycle to complete

#ifdef REALTIME
  while(1)	// This is for real operation
#else
  for(Cctr = 0; Cctr < cyclecount; Cctr++)
#endif
  {
	// This is the 1 second loop
	for(int DloopCtr = 0; DloopCtr < INTRP_RATE; DloopCtr++)
	{
		int lread;
		FD_ZERO(&readfds);
		FD_SET(sensor.getReadFd(), &readfds);
		select(1+sensor.getReadFd(), &readfds, NULL, NULL, NULL);

		lread = sensor.read(&buf, sizeof(A2DSAMPLE));

		// This is the 100 Hz loop
		for(int iii = 0; iii < RATERATIO; iii++)
		{
			//Loop over all a/d's
			for(int jjj = 0; jjj < MAXA2DS; jjj++)
			{
				ptr_inbuf[a2d->ptr[jjj]] += 
					(long)buf.data[iii][jjj];

				// If a/d active,
				if(a2d->Hz[jjj] != 0)	
				{	
					// and cycle at end,
					if(Ictr%a2d->ctr[jjj] == 0) 
					{ 
						//bump the pointer.
						a2d->ptr[jjj] += 1;	
					}
				}
			}
			Ictr++;
		}
	}
	// Reset Ictr
	Ictr = 1;
	
	//Swap buffers
	ptr_temp = ptr_outbuf;
	ptr_outbuf = ptr_inbuf;
	ptr_inbuf = ptr_temp;

	// Clear the new input buffer
	memset(ptr_inbuf, 0, bufsize);

	// Reinitialize pointers
	A2DPtrInit(a2d);

	//Normalize the output buffer contents and send
	for(j = 0; j < MAXA2DS; j++)
	{
		for(k = 0; k < a2d->Hz[j]; k++)
		{
			if(ptr_outbuf[a2d->ptr[j] + k] > 0)
				ptr_outbuf[a2d->ptr[j] + k] =
				  (long)((float)(ptr_outbuf[a2d->ptr[j] + k])*
				   a2d->norm[j] + 0.5);
			if(ptr_outbuf[a2d->ptr[j] + k] < 0)
				ptr_outbuf[a2d->ptr[j] + k] =
				  (long)((float)(ptr_outbuf[a2d->ptr[j] + k])*
				   a2d->norm[j] - 0.5);
		}
	}	

  	printf("Output buffer by rates \nsumrates = %d\n", sumrates);
  	for(jj = 0; jj < MAXA2DS; jj++)
  	{
  		for(ii = 0; ii < a2d->Hz[jj] ; ii++)
		{
			printf("a2d%1d Sample %04d: %+06ld: NormFac %8.6f\n",
				jj, ii, ptr_outbuf[a2d->ptr[jj] + ii],
				a2d->norm[jj]);
		}
  	}
  }
  run_msg = STOP;	
  sensor.ioctl(A2D_RUN_IOCTL, &run_msg, sizeof(int));
  cout << __FILE__ << ": Stop command sent to driver" << endl;
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
