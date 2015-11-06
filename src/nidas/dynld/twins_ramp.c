/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
llmain.c
void CreateRamp()
{
  int i;
  float Vout;

  printf("Creating new laser scan ramp array\n");

  // Generate full laser scan ramp over all NPTS points
  for (i=0; i<NPTS; i++ )
  {
    Vout = Vstart + ((float)i * Vstep);     // Increment laser scan voltage to  scan laser
    DAData_scan[i] = (int)(Vout * Vconvert);  // Corresponding counts
  }
  // Now form null pulse over first 30 points (ie. points 0 - 29)
  for (i=0; i<29; i++ )
  {
    DAData_scan[i] = 0;
  }
}

// Set laser scan voltage parameters
Vmin = 0.0F;      // 0-10V scans laser 0-125 mA for 20 ohm sense (unipolar    mode)
Vmax = 10.0F;
Vconvert = 32767.0F / 10.0F;    // Counts per volt
VIconvert = 10.0F / 125.0F;     // 0.08, = volts per mA for 20 ohm sense      resistor
Vstart = uNIRparms.istart * VIconvert;      // eg. 40 mA = 3.2V, 80 mA = 6.4V
Vend = Vstart + (uNIRparms.irange * VIconvert); // eg. 40 mA + 35 mA = 3.2V + 2.8V = 6.0V
Vstep = (Vend - Vstart) / (float)(NPTS - 1);  // Voltage steps for laser scan ramp

uniparm.dat
35.00 ! Starting current for laser.
50.00 ! Range of current for scan.
20    ! Number of scans to average
200   ! Number of chunks per write
35    ! Number of ARINC words to catch P/T (divide by 5)
12.43    ! R-number for short path
3.694    ! R-number for Hcell
140   ! DC offset for power calcs
150   ! left end of search region
410   ! right end of search region
0.0   ! pr1
100.0   ! pr2
0.0   ! pr3
23.0   ! tr1
0.0   ! tr2
0.0   ! tr3
68.60169     ! c0_h
0.69185      ! c1_h
3.09883e-4   ! c2_h
0     ! File write_flag (1=write, 0=don't)
0     ! 0 = ppmv, 1 = dew point (obselete ... ppmv = H2O unit)
