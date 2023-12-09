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

# include <cstdio>
# include <cstring>
# include <cerrno>
# include <cstddef>
# include <ctime>
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
    struct timeval tv;

    /*
     * Open the IRIG device
     */
    FILE *irig = fopen(devname, "r");
    if (! irig)
    {
	fprintf(stderr, "Error opening '%s': %s\n", devname, strerror(errno));
	return 1;
    }

    struct pc104sg_status status;

    if (ioctl(fileno(irig), IRIG_GET_STATUS, &status) < 0)
    {
	fprintf(stderr, "%s: IRIG_GET_STATUS error: %s\n",
            devname, strerror(errno));
	return 1;
    }
    unsigned char statusOR = status.statusOR;
    printf("%s status: %s (%#04x)\n",
	devname, IRIGSensor::statusString(statusOR).c_str(), statusOR);

    /*
     * Kick the IRIG time to now
     */
    gettimeofday(&tv, NULL);
    printf("Setting IRIG clock...");
    if (ioctl(fileno(irig), IRIG_SET_CLOCK, &tv) < 0)
    {
	fprintf(stderr, "\n%s: IRIG_SET_CLOCK error: %s\n",
            devname, strerror(errno));
	return 1;
    }
    printf("done.\n");

    UTime irigt;
    irigt.setFormat("%Y %m %d %H:%M:%S.%6f");

    UTime unixt;
    unixt.setFormat("%H:%M:%S.%6f");

    /*
     * Now just read and print times from the IRIG, which should come
     * every second.
     */
    for (int i = 0; i < 10; i++)
    {
	dsm_clock_data_3 samp3;
	dsm_clock_data_2 samp2;
	int seqnum;

	size_t lread;

        dsm_sample_time_t timetag;
        lread = sizeof(timetag);
	if (fread(&timetag, lread, 1, irig) != 1)
	{
	    fprintf(stderr, "%s: Failed to read %zu bytes\n",
                    devname, lread);
	    return 1;
	}
        dsm_sample_length_t slen;
        lread = sizeof(slen);
	if (fread(&slen, lread, 1, irig) != 1)
	{
	    fprintf(stderr, "%s: Failed to read %zu bytes\n",
                    devname, lread);
	    return 1;
	}

	if (slen == offsetof(dsm_clock_data_3, end)) {
		if (fread(&samp3, slen, 1, irig) != 1)
		{
		    fprintf(stderr, "%s: Failed to read %u bytes\n",
			    devname, slen);
		    return 1;
		}
		irigt = samp3.irigt;
		unixt = samp3.unixt;
		statusOR = samp3.status;
		seqnum = samp3.seqnum;
	}
	else if (slen == offsetof(dsm_clock_data_2, end)) {
		if (fread(&samp2, slen, 1, irig) != 1)
		{
		    fprintf(stderr, "%s: Failed to read %u bytes\n",
			    devname, slen);
		    continue;
		}
		irigt = (long long) samp2.irigt.tv_sec * USECS_PER_SEC +
			samp2.irigt.tv_usec;
		unixt = (long long) samp2.unixt.tv_sec * USECS_PER_SEC +
			samp2.unixt.tv_usec;
		statusOR = samp2.status;
		seqnum = samp2.seqnum;
	}
	else {
	    fprintf(stderr,"%s: unrecognized sample length=%u\n",
		devname, slen);
	    return 1;
	}

	double i_minus_u = (double)(irigt - unixt) / USECS_PER_MSEC;

	printf("status: %s (%#04x), irig: %s, unix: %s, irig-unix: %10.3g msec, seq=%d\n",
            IRIGSensor::statusString(statusOR).c_str(), statusOR,
            irigt.format(true).c_str(),
            unixt.format(true).c_str(),
            i_minus_u, seqnum);
    }

    return 0;
}
