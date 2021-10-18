/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2021, Copyright University Corporation for Atmospheric Research
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

/********************************************************************

  readFTDIpins.c

  Usage:

      readFTDIpins

  A simple program to read and print the state of the pins on all 
  interfaces of all 4 FTDI chips in the DSM3 serial board. Intended
  for diagnostic purposes. 

/********************************************************************

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ftdi.h>

int main(int argc, char* argv[])
{
    struct ftdi_context *ftdi;

    // enumerate our devices/interfaces...
    char* devices[4] = {"P0-P3", "P4-P7", "GPIO", "I2C"};
    enum ftdi_interface ifaces[4] = {INTERFACE_A, INTERFACE_B, INTERFACE_C, INTERFACE_D};

    if ((ftdi = ftdi_new()) == 0) {
      printf("ftdi_new failed\n");
      return 1;
    }

    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++) {
            int status = ftdi_set_interface(ftdi, ifaces[j]);
            printf("open device %s iface %d (%d)", devices[i], ifaces[j], status);
            int f = ftdi_usb_open_desc(ftdi, (int)0x0403, (int)0x6011, devices[i], 0);
            if (f<0) {
                printf("\nUnable to open device %d: (%s)", i, ftdi_get_error_string(ftdi));
		break;
            }

	    unsigned char pins = 0;
            status = ftdi_read_pins(ftdi, &pins);
            printf(" read: 0x%02hhx (%d)", pins, status);

            printf("\n");

            ftdi_usb_close(ftdi);
        }
        printf("\n");
    }
}

