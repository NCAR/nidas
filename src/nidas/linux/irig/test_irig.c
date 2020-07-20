/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
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
# include <stddef.h>
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
    struct timeval32 tv;
    struct timespec ts;
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
    clock_gettime(CLOCK_REALTIME, &ts);
    tv.tv_sec = ts.tv_sec;
    tv.tv_usec = ts.tv_nsec / NSECS_PER_SEC;
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
	size_t nread, n;
	char buf[128];
	time_t tsec;
	unsigned long usec;
        unsigned char status;

        /* There are three possible types of samples from an irig. Read
         * the sample length to determine which one is coming.
         */

        dsm_sample_time_t tt;
        n = sizeof(tt);
	if ((nread = fread(&tt, 1, n, irig)) != n) 
	{
	    fprintf(stderr, "Bad read size (%d != %d)!\n", nread, n);
	    continue;
	}
        dsm_sample_time_t len;
        n = sizeof(len);
	if ((nread = fread(&len, 1, n, irig)) != n) 
	{
	    fprintf(stderr, "Bad read size (%d != %d)!\n", nread, n);
	    continue;
	}

        if (len < offsetof(struct dsm_clock_data_2,end)) {
            struct dsm_clock_data data;
            int n = offsetof(struct dsm_clock_data,end);
            if ((nread = fread(&data, 1, n, irig)) != n) 
            {
                fprintf(stderr, "Bad read size (%d != %d)!\n", nread, n);
                continue;
            }
            status = data.status;
            tsec = data.tval.tv_sec;
            usec = data.tval.tv_usec;
        }
        else if (len < offsetof(struct dsm_clock_data_3,end)) {
            struct dsm_clock_data_2 data;
            int n = offsetof(struct dsm_clock_data_2,end);
            if ((nread = fread(&data, 1, n, irig)) != n) 
            {
                fprintf(stderr, "Bad read size (%d != %d)!\n", nread, n);
                continue;
            }
            status = data.status;
            tsec = data.irigt.tv_sec;
            usec = data.irigt.tv_usec;
        }
        else {
            struct dsm_clock_data_3 data;
            int n = offsetof(struct dsm_clock_data_3,end);
            if ((nread = fread(&data, 1, n, irig)) != n) 
            {
                fprintf(stderr, "Bad read size (%d != %d)!\n", nread, n);
                continue;
            }
            status = data.status;
            tsec = data.irigt / USECS_PER_SEC;
            usec = data.irigt % USECS_PER_SEC;
        }
	
	strftime(buf, sizeof(buf), "%F %T", gmtime(&tsec));
	printf("%s.%06ld status 0x%x\n", buf, usec, (unsigned int) status);
    }
}
