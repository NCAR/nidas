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

#ifndef USER_SPACE
#define USER_SPACE
#endif

#include <iostream>
#include <iomanip>

#include <RTL_DSMSensor.h>

#include <a2d_driver.h>

using namespace std;

int main(int argc, char** argv)
{
  A2D_SET *a2d; 	//set up a control struct
  int i;
 
  printf("\n\n-----------CK_A2D------------\n\n"__FILE__" opening sensors...\n");

  RTL_DSMSensor sensor_in_0("/dev/dsma2d0");
  RTL_DSMSensor sensor_out_0("/dev/dsma2d0");
  try {
    sensor_in_0.open(O_RDONLY);
    sensor_out_0.open(O_WRONLY);
  }
  catch (atdUtil::IOException& ioe) {
    printf("%s\n", ioe.what());
    return 1;
  }
  printf(" %s Files opened\n", __FILE__ );

  printf("Sensors opened");

/*
Load up some phoney data in the A2D control structure, A2D_SET  
*/

  for(i = 0; i < MAXA2DS; i++)
	{
	a2d->gain[i] = 2;
	a2d->ctr[i] = 0;
	a2d->Hz[i] = (US)(2*(float)i*12.5+.5);
	a2d->flag[i] = 0;
	a2d->status[i] = 0;
	a2d->ptr[i] = 0;
	}
  a2d->vcalx8 = 128;
  a2d->calset = 0;
  a2d->offset = 0;
  a2d->filter[0] = 0xA5A5;
  a2d->filter[1] = 0;

  sensor_out_0.ioctl(A2D_SET_IOCTL, a2d, sizeof(A2D_SET));	


  try {
    sensor_in_0.close();
    sensor_out_0.close();
  }
  catch (atdUtil::IOException& ioe) {
    printf("%s\n", ioe.what());
    return 1;
  }
  printf(__FILE__"  sensors closed.\n");

  printf("Sensors closed"); 
}
