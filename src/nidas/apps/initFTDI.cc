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

  initFTDI.c

  Usage:

       initFTDI [-d]    # -d logs output to syslog

  This program is intended to be invoked in response to triggering a UDEV rule tied to the FTDI devices 
  on the DSM3 serialboard. It originates out of lessons learned while investigating JIRA issue 
  #451: "pio errors and not working for bank1 and bank2".

  Recall that the serialboard on the DSM3 incorporates 4 FTDI4232HQ chips, each one containing 4 interfaces 
  (i.e, 4 sets of 8 i/o pins). The interfaces are used for various purposes, among them, to control power 
  to the serial ports and other power sources on the DSM. As such, the interfaces are used in a bit-bang 
  fashion to drive individual pins.

  There is a function in libftdi, ftdi_set_bitmode(), which is used to set the bit-bang mode on an interface. 
  It takes two arguments: the bit-bang mode, and a mask specifying which pins on the interface are considered 
  input vs. output. We determined that this function had the side-effect of resetting the state of any 
  designated output pins to zero. It further only happens on the first time ftdi_set_bitmode() is called on 
  an interface once the chip has been powered up or otherwise reset; subsequent calls to ftdi_set_bitmode() 
  do not exhibit this side-effect. (It is actually more fine-grained then that -- it occurs on a per-pin 
  basis -- the first time a "pin" on an interface is designated as output. It is not clear that the function 
  is the culprit, it may well be an intrinsic property of the FTDI chips).

  In the nidas/util source tree, there are a set of C++ classes that represent abstractions for the interfaces. 
  The base constructor for these classes incorporates a call to ftdi_set_bitmode(). Any nidas program under 
  nidas/apps that utilizes these classes could potentially cause state changes to the interface pins, if it 
  just so happened to be the first nidas program invoked after a reset on the FTDI chips. The program "pio" 
  is one scenario, as exemplified by the JIRA issue. Running "pio -v" is intended to report the status of power 
  on the various devices. But if is the first such nidas program to be invoked, it has the effect of turning power 
  off on all the devices (because the interface pins are set to zero).

  This program is intended to eliminate such pitfalls by intentionally tripping this "first time" behavior. 
  It walks through all the chip/interface pairs and i) reads the current state of the pins, ii) sets bitmode 
  with all pins designated as output, iii) resets the pins to the value that was read. By invoking this program 
  from a UDEV rule, the effect/intent is that it gets invoked early during bootup or other reset of the serialboard, 
  before any user-level nidas code would have the misfortune to trip "first time" behavior.
  
  Note that there is still a lingering side-effect: the power to the serial ports and and other sources on the DSM 
  will see a brief (miliseconds) flicker of the power as the controlling pins are reset. Not optimal, but seemingly 
  unavoidable.

  Rick Brownrigg
  October 2021

********************************************************************/

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <ftdi.h>

#include <stdlib.h>

int
main(int argc, char* argv[])
{
    struct ftdi_context *ftdi;

    // enumerate our devices/interfaces...
    #define DILEN 4
    const char* devices[DILEN] = {"P0-P3", "P4-P7", "GPIO", "I2C"};
    enum ftdi_interface ifaces[DILEN] = {INTERFACE_A, INTERFACE_B, INTERFACE_C, INTERFACE_D};

    const int DEBUG = (argc > 1 && !strcmp(argv[1], "-d")) ? 1 : 0;
    const int facPrior = LOG_MAKEPRI(LOG_USER, LOG_ERR);
    if (DEBUG) openlog(argv[0], 0, 0);

    if (DEBUG) syslog(facPrior, "ACTION=%s\n", getenv("ACTION")); 

    if ((ftdi = ftdi_new()) == 0) {
        if (DEBUG) syslog(facPrior, "ftdi_new failed\n");
        return 1;
    }

    /* iterate over all devices and their interfaces, read the current pin settings for each, 
     * set the bitmode to write and restore the value that was read.
     */
    for (int dev=0; dev<DILEN; dev++) {
        for (int iface=0; iface<DILEN; iface++) {
            ftdi_set_interface(ftdi, ifaces[iface]);
            int f = ftdi_usb_open_desc(ftdi, (int)0x0403, (int)0x6011, devices[dev], 0);
            if (f<0) {
                if (DEBUG) syslog(facPrior, "\nUnable to open device/iface %s/%d: (%s)\n", devices[dev], ifaces[iface], 
                    ftdi_get_error_string(ftdi));
                continue;
            }

            unsigned char pins = 0;
            int status_rp = ftdi_read_pins(ftdi, &pins);

            unsigned char pinMask = 0xff;   // set all pins for write

            /* INTERFACE_C on I2C is special: leave the 2 switch pins as inputs, and force the  
             * LEDs to be off.
             */
            if (dev == 3 && iface == 2) {
                pinMask = 0xcf;
                pins = pins & 0x0f;
            }
            int status_sb = ftdi_set_bitmode(ftdi, pinMask, BITMODE_BITBANG);

            int status_wd = ftdi_write_data(ftdi, &pins, 1); 

            ftdi_usb_close(ftdi);
 
            if (DEBUG) {
                syslog(facPrior, "dev=%s/iface=%d: read 0x%02hhx (%d), bitmode (%d), write (%d)", 
                    devices[dev], ifaces[iface], pins, status_rp, status_sb, status_wd);
            }
        }
    }
    ftdi_free(ftdi);
}

