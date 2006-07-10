/* ck_arinc.cc

   User space code that exercises ioctls on the ARINC module.

   Original Author: John Wasinger

   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

#include <iostream>
#include <iomanip>

#include <nidas/core/RTL_DSMSensor.h>

#include <nidas/rtlinux/cei420a.h>

using namespace std;

namespace n_u = nidas::util;

static arinc_set IRS_labels[] = {

  {1, 0007,  3}, // Time to Nav
  {1, 0010,  3}, // Pos Latitude
  {1, 0011,  3}, // Pos Longitude
  {1, 0012,  3}, // Ground Speed
  {1, 0013,  3}, // Trk Angle True
  {1, 0014,  3}, // Mag Heading
  {1, 0015,  3}, // Wind Speed
  {1, 0016,  3}, // Wind Dir True
  {1, 0044,  3}, // True Heading
  {1, 0300, 50}, // Delta Theta X
  {1, 0301, 50}, // Delta Theta Y
  {1, 0302, 50}, // Delta Theta Z
  {1, 0303, 50}, // Delta Theta V X
  {1, 0304, 50}, // Delta Theta V Y
  {1, 0305, 50}, // Delta Theta V Z
  {1, 0310, 12}, // Pos Latitude
  {1, 0311, 12}, // Pos Longitude
  {1, 0312, 25}, // Ground Speed
  {1, 0313, 25}, // Trk Angle True
  {1, 0314, 25}, // True Heading
  {1, 0315, 12}, // Wind Speed
  {1, 0316, 12}, // Wind Dir True
  {1, 0317, 25}, // Trk Angle Mag
  {1, 0320, 25}, // Mag Heading
  {1, 0321, 25}, // Drift Angle
  {1, 0322, 25}, // Flt Pth Angle
  {1, 0323, 50}, // Flt Pth Accel
  {1, 0324, 50}, // Pitch Angle
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
  std::cerr << __FILE__ << " opening sensors...\n";

  RTL_DSMSensor sensor_in_0("/dev/arinc0");
  RTL_DSMSensor sensor_in_1("/dev/arinc1");
  RTL_DSMSensor sensor_in_2("/dev/arinc2");
  RTL_DSMSensor sensor_in_3("/dev/arinc3");
  RTL_DSMSensor sensor_out_0("/dev/arinc0");
  RTL_DSMSensor sensor_out_1("/dev/arinc1");
  try {
    sensor_in_0.open(O_RDONLY);
    sensor_in_1.open(O_RDONLY);
    sensor_in_2.open(O_RDONLY);
    sensor_in_3.open(O_RDONLY);
    sensor_out_0.open(O_WRONLY);
    sensor_out_1.open(O_WRONLY);
  }
  catch (n_u::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }
  std::cerr << __FILE__ << " sensors opened.\n";


  // Send the IRS Label table to the ARINC driver.
  for (int iii = 0; iii < nIRS_labels; iii++)
  {
    std::cerr << setiosflags( ios::showbase)
              << dec << "IRS_labels[" << iii << "].channel = " << IRS_labels[iii].channel << std::endl
              << oct << "IRS_labels[" << iii << "].label   = " << IRS_labels[iii].label   << std::endl
              << dec << "IRS_labels[" << iii << "].rate    = " << IRS_labels[iii].rate    << std::endl;
    sensor_in_0.ioctl(ARINC_SET, &IRS_labels[iii], sizeof(struct arinc_set));
  }
  // Print out the status of the driver.   
  sensor_in_0.ioctl(ARINC_STAT, (void *)NULL, 0);

  // Reset the driver.   
  sensor_in_0.ioctl(ARINC_RESET, (void *)NULL, 0);

  // Print out the status of the driver.   
  sensor_in_0.ioctl(ARINC_STAT, (void *)NULL, 0);

  std::cerr << __FILE__ << " closing sensors...\n";
  try {
    sensor_in_0.close();
    sensor_in_1.close();
    sensor_in_2.close();
    sensor_in_3.close();
    sensor_out_0.close();
    sensor_out_1.close();
  }
  catch (n_u::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }
  std::cerr << __FILE__ << " sensors closed.\n";
}
