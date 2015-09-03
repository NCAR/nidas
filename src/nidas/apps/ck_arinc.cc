/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/* ck_arinc.cc

   User space code that exercises ioctls on the ARINC module.

   Original Author: John Wasinger

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

*/

#include <iostream>
#include <iomanip>

#include <sys/select.h>

#include <nidas/dynld/raf/IRS_HW_HG2001GD.h>

using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

typedef struct {
  short label;
  double rate;
} dbl_arcfg_t;

// Taken from the Honeywell installation manual for the
// Inertial Reference Unit / Part No. HG2001GD (pages 649-651).
static dbl_arcfg_t IRS_labels[] = {
  {0007,3.125}, // time_to_nav		"time to nav"
  {0010,3.125}, // pos_latitude		"pos latitude"
  {0011,3.125}, // pos_longitude	"pos longitude"
  {0012,3.125}, // ground_speed		"ground speed"
  {0013,3.125}, // trk_angle_true	"trk angle true"
  {0014,3.125}, // mag_heading_3hz	"mag heading 3.125Hz"
  {0015,3.125}, // wind_speed		"wind speed"
  {0016,3.125}, // wind_dir_true	"wind dir true"
  {0044,3.125}, // true_heading		"true heading"
  {0265,   50}, // integ_vert_accel	"integ_vert_accel"
  {0270,    2}, // irs_discretes	"irs_discretes"
  {0300,   50}, // dtheta_x		"delta theta x"
  {0301,   50}, // dtheta_y		"delta theta y"
  {0302,   50}, // dtheta_z		"delta theta z"
  {0303,   50}, // dtheta_v_x		"delta theta v x"
  {0304,   50}, // dtheta_v_y		"delta theta v y"
  {0305,   50}, // dtheta_v_z		"delta theta v z"
  {0310, 12.5}, // LAT			"IRS Latitude"
  {0311, 12.5}, // LON			"IRS Longitude"
  {0312,   25}, // GSF			"IRS Aircraft Ground Speed"
  {0313,   25}, // TKAT			"IRS Aircraft Track Angle"
  {0314,   25}, // THDG			"IRS Aircraft True Heading Angle"
  {0315, 12.5}, // IWS			"IRS Wind Speed"
  {0316, 12.5}, // IWD			"IRS Wind Direction"
  {0317,   25}, // TKAM			"trk angle mag"
  {0320,   25}, // mag_heading_25hz	"mag heading 25Hz"
  {0321,   25}, // DRFTA		"IRS Drift Angle"
  {0322,   25}, // flt_pth_angle	"flt pth angle"
  {0323,   50}, // flt_pth_accel	"flt pth accel"
  {0324,   50}, // PITCH		"IRS Aircraft Pitch Angle"
  {0325,   50}, // ROLL			"IRS Aircraft Roll Angle"
  {0326,   50}, // BPITCHR		"IRS Body Pitch Rate"
  {0327,   50}, // BROLLR		"IRS Body Roll Rate"
  {0330,   50}, // BYAWR		"IRS Body Yaw Rate"
  {0331,   50}, // BLONGA		"IRS Body Longitudal Acceleration"
  {0332,   50}, // BLATA		"IRS Body Latitudal Acceleration"
  {0333,   50}, // BNORMA		"IRS Body Normal Acceleration"
  {0334,   25}, // PHDG			"IRS Platform Heading"
  {0335,   50}, // TKAR			"IRS Track Angle Rate"
  {0336,   50}, // pitch_att_rate	"pitch_att_rate"
  {0337,   50}, // roll_att_rate	"roll_att_rate"
  {0350,3.125}, // irs_maint_disc	"irs_maint_discretes"
  {0351,3.125}, // time_to_nav_rdy	"time_to_nav_ready"
  {0354,   50}, // total_time		"total time"
  {0360,   50}, // pot_vert_speed	"pot_vert_speed"
  {0361,   25}, // ALT			"IRS Altitude"
  {0362,   50}, // along_trk_accel	"along trk accel"
  {0363,   50}, // cross_trk_accel	"cross trk accel"
  {0364,   50}, // ACINS		"IRS Vertical Acceleration"
  {0365,   50}, // VSPD			"IRS Vertical Speed"
  {0366, 12.5}, // VNS			"IRS Ground Speed Vector, North Component"
  {0367, 12.5}, // VEW			"IRS Ground Speed Vector, East Component"
  {0370, 6.25}, // norm_accel		"norm_accel"
  {0371,3.125}, // equipment_id		"equipment_id"
  {0375,   50}, // along_hdg_accel	"along hdg accel"
  {0376,   50}, // cross_hdg_accel	"cross hdg accel"
};
static int nIRS_labels = sizeof(IRS_labels) / sizeof(dbl_arcfg_t);

int main() //int argc, char** argv)
{
  std::cerr << __FILE__ << " creating sensors...\n";

  IRS_HW_HG2001GD sensor_in_0;
  sensor_in_0.setDeviceName("/dev/arinc0");
  IRS_HW_HG2001GD sensor_in_1;
  sensor_in_1.setDeviceName("/dev/arinc1");
  IRS_HW_HG2001GD sensor_in_2;
  sensor_in_2.setDeviceName("/dev/arinc2");
  IRS_HW_HG2001GD sensor_in_3;
  sensor_in_3.setDeviceName("/dev/arinc3");

  std::cerr << __FILE__ << " opening sensors...\n";
  try {
    sensor_in_0.open(O_RDONLY);
    sensor_in_1.open(O_RDONLY);
    sensor_in_2.open(O_RDONLY);
    sensor_in_3.open(O_RDONLY);
  }
  catch (n_u::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }
  std::cerr << __FILE__ << " sensors opened.\n";
/*
  // Send the IRS Label table to the ARINC driver.
  for (int iii = 0; iii < nIRS_labels; iii++)
  {
    arcfg_t arcfg;
    arcfg.label = IRS_labels[iii].label;
    // round down the floating point rates
    arcfg.rate  = (short) floor(IRS_labels[iii].rate);

    std::cerr << setiosflags( ios::showbase)
              << oct << "arcfg.label = " << arcfg.label   << std::endl
              << dec << "arcfg.rate  = " << arcfg.rate    << std::endl;
    sensor_in_0.ioctl(ARINC_SET, &arcfg, sizeof(arcfg_t));
  }
  // Print out the status of the driver.   
  sensor_in_0.ioctl(ARINC_STAT, (void *)NULL, 0);

  // Print out the status of the driver.   
  sensor_in_0.ioctl(ARINC_STAT, (void *)NULL, 0);
*/
  std::cerr << __FILE__ << " closing sensors...\n";
  try {
    sensor_in_0.close();
    sensor_in_1.close();
    sensor_in_2.close();
    sensor_in_3.close();
  }
  catch (n_u::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }
  std::cerr << __FILE__ << " sensors closed.\n";
  return 0;
}
