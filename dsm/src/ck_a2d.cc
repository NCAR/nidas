/* ck_a2d.cc

   User space code that exercises ioctls on the a2d_driver module.

   Original Author: Grant Gray

   Copyright by the National Center for Atmospheric Research


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

#include <ioctl_fifo.h>
#include <a2d_driver.h>
#include <RTL_DSMSensor.h>

using namespace std;

void loada2dstruct(A2D_SET *a);
void A2D_SETDump(A2D_SET *a);
int  sleepytime = 1;


int main(int argc, char** argv)
{
  A2D_SET *a2d; 	//set up a control struct
  UL run_msg;
  US inbuf[MAXA2DS*INTRP_RATE + 4];
  int ii, jj, i;

  fd_set readfds;

  if(argc > 1)sleepytime = atoi(argv[1]);
  if(sleepytime > 10000 || sleepytime < 0)sleepytime = 1;

  cout << endl << endl;
  cout << "----CK_A2D----" << endl; 
  cout << __FILE__ << ": Creating sensor class..." << endl;

  RTL_DSMSensor sensor("/dev/dsma2d0");

  try {
    sensor.open(O_RDONLY);
  }
  catch (atdUtil::IOException& ioe) {
    cerr << ioe.what() << endl;    
    return 1;
  }

  cout << __FILE__ << ": Up Fifo opened" << endl;

  // Load some phoney data into the a2d structure
  a2d = (A2D_SET *)malloc(sizeof(A2D_SET));
  loada2dstruct(a2d);
  cout << __FILE__ << ": Structure loaded" << endl;
  cout << __FILE__ << ": Size of A2D_SET = " << sizeof(A2D_SET) << endl;

  //Send the struct to the a2d_driver
 
  sensor.ioctl(A2D_SET_IOCTL, a2d, sizeof(A2D_SET));	

  cout << __FILE__ << ": Initialization data sent to driver" << endl;

  a2d->calset = 0x80;
  a2d->offset = 0x10;

  A2D_SETDump(a2d);

  sensor.ioctl(A2D_CAL_IOCTL, a2d, sizeof(A2D_SET));

  cout << __FILE__ << ": Calibration command sent to driver" << endl;

  run_msg = RUN;	

  sensor.ioctl(A2D_RUN_IOCTL, &run_msg, sizeof(int));
  
  cout << __FILE__ << ": Run command sent to driver" << endl;

  for(i = 0; i < sleepytime; i++)
  {
        int lread;
/*
	FD_ZERO(&readfds);
	FD_SET(sensor.getReadFd(), &readfds);
	select(1, &readfds, NULL, NULL, NULL);
*/
	lread = sensor.read(inbuf, 2*MAXA2DS*INTRP_RATE + 8);
//      cerr << "lread=" << lread << endl;
	
	printf("0x%04d%04d\n", inbuf[1], inbuf[0]);
	printf("0x%04d%04d\n", inbuf[3], inbuf[2]);

	for(ii = 0; ii < 10 ; ii++)
	{
		for(jj = 0; jj < MAXA2DS; jj++)
		{
			printf("%05d  ", inbuf[MAXA2DS*ii + jj + 4]);
		}
		printf("\n");	
	}
	printf("index = %6d\n", i);
  }


//  sleep(sleepytime);

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

  	for(i = 0; i < MAXA2DS; i++)
		{
		a2d->gain[i] = i;
		a2d->ctr[i] = 0;
		a2d->Hz[i] = (US)(2*(float)(i/4+1)*25+.5);
		a2d->flag[i] = 0;
		a2d->status[i] = 0;
		a2d->ptr[i] = 0;
		}
 	a2d->vcalx8 = 128;
	a2d->calset = 0;
 	a2d->offset = 0;
	if((fp = fopen("filtercoeff.bin", "rb")) == NULL);
		{
		printf("No filter file!\n");
//		exit(1);
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
		printf("Gain_%1d  = %3d\t", i, a2d->gain[i]);
		printf("Hz_%1d    = %3d\n", i, a2d->Hz[i]);
		}

	printf("Vcalx8  = %3d\n", a2d->vcalx8);
	printf("filter0 = 0x%04X\n", a2d->filter[0]);

	return;
}
