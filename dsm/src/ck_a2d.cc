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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <iomanip>

#include <ioctl_fifo.h>
#include <a2d_driver.h>
#include <RTL_DSMSensor.h>

using namespace std;

void loada2dstruct(A2D_SET *a);
void A2D_SETDump(A2D_SET *a);


int main(int argc, char** argv)
{
  A2D_SET *a2d; 	//set up a control struct
 

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
  A2D_SETDump(a2d);
  //Send the struct to the a2d_driver
 
  sensor.ioctl(A2D_SET_IOCTL, a2d, sizeof(A2D_SET));	

  cout << __FILE__ << ": Initialization data sent to driver" << endl;

  a2d->calset = 0x80;
  a2d->offset = 0x10;

  A2D_SETDump(a2d);

  sensor.ioctl(A2D_CAL_IOCTL, a2d, sizeof(A2D_SET));

  cout << __FILE__ << ": Calibration data sent to driver" << endl;

}

void loada2dstruct(A2D_SET *a2d)
{
	int i;
	
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
	a2d->filter[0] = 0xA5A5;
  	a2d->filter[1] = 0;
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

	printf("Vcal    = %3d\n", a2d->vcalx8);
	printf("filter0 = 0x%04X\n", a2d->filter[0]);

	return;
}
