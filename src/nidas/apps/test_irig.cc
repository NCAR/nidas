/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
# include <stdio.h>
# include <string.h>
# include <errno.h>
# include <time.h>
# include <sys/ioctl.h>
# include <nidas/util/UTime.h>
# include <nidas/dynld/raf/IRIGSensor.h>
# include <nidas/linux/irigclock.h>

using nidas::util::UTime;
using nidas::dynld::raf::IRIGSensor;

int
main() 
{
    const char *devname = "/dev/irig0";
    FILE *irig = fopen(devname, "r");
    int status;
    struct timeval tv;
    /*
     * Open the IRIG device
     */
    if (! irig)
    {
	fprintf(stderr, "Error opening '%s': %s\n", devname, strerror(errno));
	return 1;
    }
    /*
     * Kick the IRIG time to now
     */
    gettimeofday(&tv, NULL);
    printf("Kicking IRIG clock...\n");
    if ((status = ioctl(fileno(irig), IRIG_SET_CLOCK, &tv)) != sizeof(tv))
    {
	fprintf(stderr, "IRIG_SET_CLOCK error: %s\n", strerror(-status));
	return 1;
    }
    printf("done.\n");

    UTime irigt;
    irigt.setFormat("%Y %m %d %H:%M:%S.%3f");

    UTime unixt;
    unixt.setFormat("%Y %m %d %H:%M:%S.%3f");

    bool onereadOK = false;

    /*
     * Now just read and print times from the IRIG, which should come
     * every second.
     */
    for (int i = 0; i < 10; i++) 
    {
	struct dsm_clock_sample_3 samp;
	unsigned int nread;

	if ((nread = fread(&samp, 1, sizeof(samp), irig)) != 1) 
	{
	    fprintf(stderr, "Bad read size (%u != %lu)!\n", nread, 
		    sizeof(samp));
	    continue;
	}

        onereadOK = true;

        unsigned char status = samp.data.status;
	
	irigt = samp.data.irigt;
	unixt = samp.data.unixt;
        double i_minus_u = (double)(irigt - unixt) / USECS_PER_SEC;

	printf("irig: %s (%#04x), time: %s, irig-unix: %10.3g sec\n",
            IRIGSensor::statusString(status).c_str(), status,
            irigt.format(true).c_str(),
            i_minus_u);
    }

    return onereadOK ? 0 : 1;
}
