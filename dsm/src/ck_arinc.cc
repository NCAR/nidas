/* ck_arinc.cc

   User space code that exercises ioctls on the ARINC module.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Revisions:

     $LastChangedRevision$
         $LastChangedDate: $
           $LastChangedBy$
                 $HeadURL: $
*/

#include <iostream>
#include <iomanip>

#include <RTL_DSMSensor.h>

#include <cei420a.h>

using namespace std;

static arinc_set IRS_labels[] = {
  {1, 0325, 50}, // Roll Angle
  {1, 0326, 50}, // Pitch Rate
  {1, 0327, 50}, // Roll Rate
  {1, 0330, 50}, // Yaw Rate
  {1, 0331, 50}, // Long Accel
  {1, 0332, 50}, // Lat Accel
  {1, 0333, 50}, // Norm Accel
  {1, 0334, 25}, // Plat Heading
  {1, 0335, 50}, // Trk Angle Rate
  {1, 0336, 50}, // Pitch Att Rate
  {1, 0337, 50}, // Roll Att Rate
  {1, 0350,  3}, // IRS Maintenance
  {1, 0351,  3}, // Time to NAVRDY
  {1, 0354, 50}, // Total Time
  {1, 0360, 50}, // Ptnl Vrt Speed
  {1, 0361, 25}, // Inertial Altitude
  {1, 0362, 50}, // Along Trk Accel
  {1, 0363, 50}, // Cross Trk Accel
  {1, 0364, 50}, // Vertical Accel
  {1, 0365, 50}, // Inrt Vrt Speed
  {1, 0366, 12}, // N-S Velocity
  {1, 0367, 12}, // E-W Velocity
  {1, 0370,  6}, // Unbias Norm Accel
  {1, 0371,  3}, // Equipment ID
  {1, 0375, 50}, // Along Heading Accel
  {1, 0376, 50}, // Cross Heading Accel
};
static int nIRS_labels = sizeof(IRS_labels) / sizeof(struct arinc_set);

int main(int argc, char** argv)
{
  RTL_DSMSensor sensor("/dev/arinc0");
  try {
    sensor.open(O_RDONLY);
  }
  catch (atdUtil::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }

  // Send the IRS Label table to the ARINC driver.
  for (int iii = 0; iii < nIRS_labels; iii++)
  {
    std::cerr << setiosflags( ios::showbase)
              << dec << "IRS_labels[" << iii << "].channel = " << IRS_labels[iii].channel << std::endl
              << oct << "IRS_labels[" << iii << "].label   = " << IRS_labels[iii].label   << std::endl
              << dec << "IRS_labels[" << iii << "].rate    = " << IRS_labels[iii].rate    << std::endl;
    sensor.ioctl(ARINC_SET, &IRS_labels[iii], sizeof(struct arinc_set));
  }
  // Print out the status of the driver.   
  sensor.ioctl(ARINC_STAT, (void *)NULL, 0);

  // Reset the driver.   
  sensor.ioctl(ARINC_RESET, (void *)NULL, 0);

  // Print out the status of the driver.   
  sensor.ioctl(ARINC_STAT, (void *)NULL, 0);
}
