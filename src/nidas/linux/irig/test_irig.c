/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
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
# include <sys/time.h>
# include <nidas/linux/irigclock.h>

int
main() 
{
    char *devname = "/dev/irig0";
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
    /*
     * Now just read and print times from the IRIG, which should come
     * every second.
     */
    while (1) 
    {
	struct dsm_clock_sample samp;
	int nread;
	char buf[128];
	time_t tsec;
	
	if ((nread = fread(&samp, sizeof(samp), 1, irig)) != 1) 
	{
	    fprintf(stderr, "Bad read size (%d != %d)!\n", nread, 
		    sizeof(samp));
	    continue;
	}
	
	tsec = samp.data.tval.tv_sec;
	strftime(buf, sizeof(buf), "%F %T", gmtime(&tsec));
	printf("%s.%06ld status 0x%x\n", buf, 
	       (unsigned long)samp.data.tval.tv_usec, 
	       samp.data.status);
    }
}
