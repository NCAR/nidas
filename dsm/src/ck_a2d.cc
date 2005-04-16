// #define DEBUGFILT
/* ck_a2d.cc

   User space code that exercises ioctls on the a2d_driver module.

   Original Author: Grant Gray

   Copyright 2005 UCAR, NCAR, All Rights Reserved


   Revisions:

     $LastChangedRevision: 695 $
         $LastChangedDate: $
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
using namespace dsm;


void loada2dstruct(A2D_SET *a);
void A2D_SETDump(A2D_SET *a);
int  sleepytime = 1, printmodulus = 1;
char *filtername = "fir10KHz.cfg";


int main(int argc, char** argv)
{
  A2D_SET *a2d; 	//set up a control struct 
  A2DSAMPLE buf; 	// and a data struct
  UL run_msg;
  int ii, jj, i;

  fd_set readfds;

  if(argc < 2)
  {
	printf("\n\nUsage:\nck_a2d <#cycles> [print interval]\n");
	printf("Note print interval defaults to 1\n");
		
	printf("\nExample:\nck_a2d 8 2 will do 8 cycles printing every other cycle.\n\n");
	exit(0);
  }
  else	
	sleepytime = atoi(argv[1]);

  if(sleepytime < 0)sleepytime = 1;

  if(argc > 2)
  {
	sleepytime += 1;	// Bump this to print last one
	printmodulus = atoi(argv[2]);
  	if(printmodulus > sleepytime)printmodulus = sleepytime;
  }


    if(argc == 4)
    {
        strcpy(filtername, argv[3]);
    }

    printf("Using filter file %s \n", filtername);

  cerr << endl << endl;
  cerr << "----CK_A2D----" << endl; 
  cerr << __FILE__ << ": Creating sensor class ..." << endl;

  RTL_DSMSensor sensor;

  cerr << "setting Device name" << endl;
  sensor.setDeviceName("/dev/dsma2d0");
  cerr << "opening" << endl;

  try {
    sensor.open(O_RDONLY);
  }
  catch (atdUtil::IOException& ioe) {
    cerr << ioe.what() << endl;    
    return 1;
  }

  cerr << __FILE__ << ": Up Fifo opened" << endl;

  // Load some phoney data into the a2d structure
  a2d = (A2D_SET *)malloc(sizeof(A2D_SET));
  loada2dstruct(a2d);
  cerr << __FILE__ << ": Structure loaded" << endl;
  cerr << __FILE__ << ": Size of A2D_SET = " << sizeof(A2D_SET) << endl;

  //Send the struct to the a2d_driver
 
  sensor.ioctl(A2D_SET_IOCTL, a2d, sizeof(A2D_SET));	

  cerr << __FILE__ << ": Initialization data sent to driver" << endl;

  for(int iset = 0; iset < MAXA2DS; iset++)
  {
  if(iset == 0)a2d->calset[iset] = 1;
  else a2d->calset[iset] = 0;
  if(iset == 1)a2d->offset[iset] = 1;
  else a2d->offset[iset] = 0;
  }

  A2D_SETDump(a2d);
/*
  sensor.ioctl(A2D_CAL_IOCTL, a2d, sizeof(A2D_SET));

  cerr << __FILE__ << ": Calibration command sent to driver" << endl;
*/
  run_msg = RUN;	

  sensor.ioctl(A2D_RUN_IOCTL, &run_msg, sizeof(int));
  
  cerr << __FILE__ << ": Run command sent to driver" << endl;

  usleep(10000); // Wait for one 100 Hz cycle to complete

  for(i = 0; i <  sleepytime; i++)
  {
	int lread;

	FD_ZERO(&readfds);
	FD_SET(sensor.getReadFd(), &readfds);
	select(1+sensor.getReadFd(), &readfds, NULL, NULL, NULL);

	lread = sensor.read(&buf, sizeof(A2DSAMPLE));
	cerr << "sizeof a2dsample = " << sizeof(A2DSAMPLE);
//	cerr << "lread=" << lread << endl;

	if(i%printmodulus == 0)
	{	
		printf("\n\nindex = %6d\n", i);
		printf("0x%08lX\n", buf.timestamp);
//		printf("0x%08X\n", (UL)buf.size);

		for(ii = 0; ii < INTRP_RATE ; ii++)
		{
			printf("0x%05X: ", ii*MAXA2DS);
			for(jj = 0; jj < MAXA2DS; jj++)
			{
				printf("%05d  ", buf.data[ii][jj]);
			}
			printf("\n");	
		}
	}
  }

  run_msg = STOP;	

  sensor.ioctl(A2D_RUN_IOCTL, &run_msg, sizeof(int));
  
  cerr << __FILE__ << ": Stop command sent to driver" << endl;


}

void loada2dstruct(A2D_SET *a2d)
{
	int i;
	FILE *fp;
	char fline[80];
	
/*
Load up some phoney data in the A2D control structure, A2D_SET  
*/

  	for(i = 0; i < MAXA2DS; i++)
	{
		a2d->gain[i] = i;
		a2d->ctr[i] = 0;
		a2d->Hz[i] = (US)(2*(float)(i/4+1)*25+.5);
		a2d->status[i] = 0;
		a2d->ptr[i] = 0;
		a2d->calset[i] = 0;
 		a2d->offset[i] = 0;
	}
 	a2d->vcalx8 = 128;

	if((fp = fopen(filtername, "r")) == NULL)
	{
		printf("No filter file!\n");
		exit(1);
	}
	
    for(i = 0; i < CONFBLLEN*CONFBLOCKS + 1; i++)
    {
        fscanf(fp, "%4x\n", &a2d->filter[i]);

#ifdef DEBUGFILT
		if(i%10==0)printf("\n%03d: ", i);
        printf(" %04X", a2d->filter[i]);
#endif
    }

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
		printf("Gain_%1d  = %3d\t", i, a2d->gain[i]);
		printf("Hz_%1d    = %3d\n", i, a2d->Hz[i]);
	}

	printf("Vcalx8  = %3d\n", a2d->vcalx8);
	printf("filter0 = 0x%04X\n", a2d->filter[0]);

	return;
}
