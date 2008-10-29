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

#include <nidas/core/DSMSensor.h>
#include <nidas/core/RTL_IODevice.h>

#include <nidas/rtlinux/arinc.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

static arcfg_t IRS_labels[] = {

  {0007,  3}, // Time to Nav
  {0010,  3}, // Pos Latitude
  {0011,  3}, // Pos Longitude
  {0012,  3}, // Ground Speed
  {0013,  3}, // Trk Angle True
  {0014,  3}, // Mag Heading
  {0015,  3}, // Wind Speed
  {0016,  3}, // Wind Dir True
  {0044,  3}, // True Heading
  {0300, 50}, // Delta Theta X
  {0301, 50}, // Delta Theta Y
  {0302, 50}, // Delta Theta Z
  {0303, 50}, // Delta Theta V X
  {0304, 50}, // Delta Theta V Y
  {0305, 50}, // Delta Theta V Z
  {0310, 12}, // Pos Latitude
  {0311, 12}, // Pos Longitude
  {0312, 25}, // Ground Speed
  {0313, 25}, // Trk Angle True
  {0314, 25}, // True Heading
  {0315, 12}, // Wind Speed
  {0316, 12}, // Wind Dir True
  {0317, 25}, // Trk Angle Mag
  {0320, 25}, // Mag Heading
  {0321, 25}, // Drift Angle
  {0322, 25}, // Flt Pth Angle
  {0323, 50}, // Flt Pth Accel
  {0324, 50}, // Pitch Angle
  {0325, 50}, // Roll Angle
  {0326, 50}, // Pitch Rate
  {0327, 50}, // Roll Rate
  {0330, 50}, // Yaw Rate
  {0331, 50}, // Long Accel
  {0332, 50}, // Lat Accel
  {0333, 50}, // Norm Accel
  {0334, 25}, // Plat Heading
  {0335, 50}, // Trk Angle Rate
  {0336, 50}, // Pitch Att Rate
  {0337, 50}, // Roll Att Rate
  {0350,  3}, // IRS Maintenance
  {0351,  3}, // Time to NAVRDY
  {0354, 50}, // Total Time
  {0360, 50}, // Ptnl Vrt Speed
  {0361, 25}, // Inertial Altitude
  {0362, 50}, // Along Trk Accel
  {0363, 50}, // Cross Trk Accel
  {0364, 50}, // Vertical Accel
  {0365, 50}, // Inrt Vrt Speed
  {0366, 12}, // N-S Velocity
  {0367, 12}, // E-W Velocity
  {0370,  6}, // Unbias Norm Accel
  {0371,  3}, // Equipment ID
  {0375, 50}, // Along Heading Accel
  {0376, 50}, // Cross Heading Accel
};
static int nIRS_labels = sizeof(IRS_labels) / sizeof(arcfg_t);

class TestArinc : public DSMSensor
{
public:
  IODevice* buildIODevice() throw(n_u::IOException)
  {
    return new RTL_IODevice();
  }
  SampleScanner* buildSampleScanner()
  {
    return new DriverSampleScanner();
  }
};

int main(int argc, char** argv)
{
  std::cerr << __FILE__ << " opening sensors...\n";

  TestArinc sensor_in_0;
  sensor_in_0.setDeviceName("/dev/arinc0");
  TestArinc sensor_in_1;
  sensor_in_1.setDeviceName("/dev/arinc1");
  TestArinc sensor_in_2;
  sensor_in_2.setDeviceName("/dev/arinc2");
  TestArinc sensor_in_3;
  sensor_in_3.setDeviceName("/dev/arinc3");
  TestArinc sensor_out_0;
  sensor_out_0.setDeviceName("/dev/arinc0");
  TestArinc sensor_out_1;
  sensor_out_1.setDeviceName("/dev/arinc1");
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
              << oct << "IRS_labels[" << iii << "].label   = " << IRS_labels[iii].label   << std::endl
              << dec << "IRS_labels[" << iii << "].rate    = " << IRS_labels[iii].rate    << std::endl;
    sensor_in_0.ioctl(ARINC_SET, &IRS_labels[iii], sizeof(arcfg_t));
  }
  // Print out the status of the driver.   
  sensor_in_0.ioctl(ARINC_STAT, (void *)NULL, 0);

  // Reset the driver.   
  sensor_in_0.ioctl(ARINC_CLOSE, (void *)NULL, 0);

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
  return 0;
}
