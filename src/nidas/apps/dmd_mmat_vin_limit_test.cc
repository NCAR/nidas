// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
//#include <cmath>
#include <nidas/linux/diamond/dmd_mmat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define NUM_ADC_TEST_CHANNELS_DEFAULT 16
#define ADC_SAMPLE_RATE_DEFAULT 10
#define ADC_GAIN_DEFAULT 2 // 1:+/-10V range, 2:+/-5V range
#define V_LIMIT_HIGH_DEFAULT 2.510
#define V_LIMIT_LOW_DEFAULT  2.490


void print_app_usage(void)
{
    printf("\n--- Constant Voltage Limit Test of Analog Channels ---\n");
    printf("[-h] [-v] [-W] [-H vlim_h] [-L vlim_l] [-N num_channels] [-C channel] [-F sample_rate] -d device\n");
    printf("-h          Help\n");
    printf("-v          Verbose\n");
    printf("-w          Wide Input Range, +/-10V. Default=+/-5V\n");
    printf("-H (float)  High Input Voltage Limit, Default=%f\n", V_LIMIT_HIGH_DEFAULT);
    printf("-L (float)  Low  Input Voltage Limit, Default=%f\n", V_LIMIT_LOW_DEFAULT);
    printf("-N (uint)   Number of ADC Channels to Test, 0 to N-1, Default=%d\n", NUM_ADC_TEST_CHANNELS_DEFAULT);
    printf("-C (uint)   Specify Single ADC Channel to Test only, Overrides -N parameter\n");
    printf("-F (uint)   ADC Sample Rate[Hz] , Default=%d\n", ADC_SAMPLE_RATE_DEFAULT);
    printf("-d (str)    ADC device, i.e. /dev/dmmat_a2d0\n");
    printf("Note: Assumes 16-bit ADC with +/-5V Input Range, no filtering\n");
    printf("Note: Likely requires root privileges\n");
    printf("Example: %% ./dmd_mmat_vin -v -L 2.490 -H 2.510 -N 8 -F 100 -d /dev/dmmat_a2d0\n");
    return;
}

// Codes to Volts convers, hard-coded for 16-bit 2's complement
//  gain=1:+/-10V range, =2:+/-5V range
float ctov(int code, int gain)
{
    if ((code > 32767) || (code < -32768))
        return -32768;

    return (10.0 / gain * code / 32768);
}

int main (int argc, char **argv)
{
    int fd;
    char a2d_dev[20];
    struct nidas_a2d_config cfg; 
    struct nidas_a2d_sample_config scfg;
    struct dsm_sample header;
    struct dsm_sample * p_sample;

    struct DMMAT_A2D_Status stat;
    int num_channels = 0; // Supported by card
    ssize_t rtn_read  = 0;
    int     rtn_ioctl = 0;

    float vlim_high = V_LIMIT_HIGH_DEFAULT;
    float vlim_low  = V_LIMIT_LOW_DEFAULT;
    unsigned int sample_rate = ADC_SAMPLE_RATE_DEFAULT;
    unsigned int adc_gain = ADC_GAIN_DEFAULT;
    unsigned int test_chan_start = 0;
    unsigned int test_chan_end   = NUM_ADC_TEST_CHANNELS_DEFAULT-1;
    unsigned int verbose_enabled = 0;

    // Parse Arguments
    extern char *optarg; /* set by getopt() */
    int opt_char;        /* option character */

    // Parse inputs args
    while ((opt_char = getopt(argc, argv, "hvwd:H:L:N:C:F:")) != -1)
    {
        switch (opt_char)
        {
            case 'v' :
                verbose_enabled = 1;
            break;

            case 'w' :
                adc_gain = 1; // gain=1, +/-10V input range when bipolar
            break;

            case 'd' :
                if (strlen(optarg) < 20-1)
                    strcpy(a2d_dev, optarg);
                else
                {
                    printf("ERROR str too long: %s\n", optarg);
                    exit(1);
                }
            break;

            case 'H' :
                vlim_high = atof(optarg);
                if ((vlim_high > 10.0) || (vlim_low < -10.0))
                {
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'L' :
                vlim_low = atof(optarg);
                if ((vlim_low > 10.0) || (vlim_low < -10.0))
                {
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'N' :
                test_chan_start = 0;
                test_chan_end   = atoi(optarg) - 1;
                if (test_chan_end > MAX_A2D_CHANNELS - 1)
                {
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'C' :
                test_chan_start = atoi(optarg);
                if (test_chan_start > MAX_A2D_CHANNELS - 1)
                {
                    print_app_usage();
                    exit(1);
                }
                test_chan_end   = test_chan_start;
            break;

            case 'F' :
                sample_rate = atoi(optarg);
                if ((sample_rate > 10000) || (sample_rate < 1))
                {
                    printf("ERROR: max sample rate 10000 Hz");
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'h' :
                print_app_usage();
                exit(1);
            break;

            case '?' :
            default :
                print_app_usage();
                printf("ERROR bad input %c\n", opt_char);
                exit(1);
            break;

        }
    }
    
    if (vlim_low > vlim_high)
    {
        printf("ERROR: vlim_high < vlim_low");
        exit(1);
    }

    printf("Testing Analog Inputs...\n");

    // Open device
    if (verbose_enabled)
        printf("Opening Device...");

    fd = open(a2d_dev, O_RDWR);
    if (fd < 0)
    {
        printf("FAILED\n");
        exit(1);
    }
    else if (verbose_enabled)
        printf("fd=%d, SUCCESS\n", fd);

    // Configure Device
    if (verbose_enabled)
        printf("Number of Channels...");

    rtn_ioctl = ioctl(fd, NIDAS_A2D_GET_NCHAN, &num_channels);
    if (rtn_ioctl < 0)
    {
        printf("ERROR: ioctl NIDAS_A2D_GET_NCHAN\n");
        exit(1);
    }
    else if (verbose_enabled)
        printf("%d\n", num_channels);

    cfg.scanRate = sample_rate;
    cfg.latencyUsecs = 10;
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10; // TODO: Why?

    if (verbose_enabled)
        printf("Configuring Device...");

    rtn_ioctl = ioctl(fd, NIDAS_A2D_SET_CONFIG, &cfg);
    if (rtn_ioctl < 0)
    {
        printf("ERROR: ioctl NIDAS_A2D_SET_CONFIG\n");
        exit(1);
    }

    // Setup Sample Configuration
    scfg.sindex = 0;
    scfg.nvars = test_chan_end+1;
    scfg.rate = sample_rate; // TODO: Correctly set sampling rate and scan rate
    scfg.filterType = NIDAS_FILTER_PICKOFF;
    for (int i = 0; i < MAX_A2D_CHANNELS; i++)
    {
        scfg.channels[i] = i; // Set variable as channel
        scfg.gain[i] = adc_gain;  // adc gain: 2->+/-5V
        scfg.bipolar[i] = 1;  // +/- bipolar
    }
    scfg.nFilterData = 0; // No Filter data

    for (int j = 0; j < scfg.nvars; j++)
    {
        if (scfg.channels[j] >= num_channels)
        {
            printf("Ch.%d is out of range, max=%d\n", scfg.channels[j], num_channels);
            exit(1);
        }
    }

    rtn_ioctl = ioctl(fd, NIDAS_A2D_CONFIG_SAMPLE, &scfg);
    if (rtn_ioctl < 0)
    {
        printf("ERROR: ioctl NIDAS_A2D_CONFIG_SAMPLE, %d\n", rtn_ioctl);
        exit(1);
    }
    else if (verbose_enabled)
        printf("SUCCESS\n");

    // Start Device
    if (verbose_enabled)
        printf("Starting Device...");

    rtn_ioctl = ioctl(fd, DMMAT_START,0);
    if (rtn_ioctl < 0)
    {
        printf("ERROR: ioctl DMMAT_START, %d\n",rtn_ioctl);
        exit(1);
    }
    else if (verbose_enabled)
        printf("SUCCESS\n");

    // Wait for buffer to fill
    if (verbose_enabled)
        printf("Waiting...\n");
    sleep(1);

    // Get Data (dsm_sample, variable length)
    if (verbose_enabled)
        printf("Reading Data...");

    rtn_read = read(fd, &header, SIZEOF_DSM_SAMPLE_HEADER);
    p_sample = (struct dsm_sample *) malloc(sizeof(struct dsm_sample) + header.length);
    rtn_read = read(fd, p_sample->data, header.length) | rtn_read;
    p_sample->timetag = header.timetag;
    p_sample->length = header.length;

    if (rtn_read < 0)
    {
        printf("ERROR: read A2D, rtn=%zd errno=%d\n", rtn_read, errno);
        exit(1);
    }
    else if (verbose_enabled)
        printf("SUCCESS\n");

    // Check Status
    if (verbose_enabled)
        printf("Checking Status...");

    rtn_ioctl = ioctl(fd, DMMAT_A2D_GET_STATUS, &stat);
    if (rtn_ioctl < 0)
    {
        printf("A2D Status Read Exception, %d\n", rtn_ioctl);
        exit(1);
    }
    else if (verbose_enabled)
    {
        printf("SUCCESS\n");
        printf("FIFO: empty=%d, over=%d, under=%d, lostSamples=%d\n", 
                stat.fifoEmpty, stat.fifoOverflows, stat.fifoUnderflows, stat.missedSamples);
    }

    // Convert Data to Codes and Voltage
    //   When copying dsm_sample over to short array, ignore 1st short (blank sample counter)
    short int adc_codes[MAX_A2D_CHANNELS];
    float adc_volts[MAX_A2D_CHANNELS];
    memcpy(adc_codes, p_sample->data + sizeof(short int), sizeof(adc_codes));    
    for (unsigned int i = test_chan_start; i <= test_chan_end; i++)
    {
        adc_volts[i] = ctov(adc_codes[i], adc_gain);
    }

    // Print Data
    if (verbose_enabled)
    {
        printf("%d Bytes\n", header.length);
        printf("Codes: ");
        for (unsigned int i = test_chan_start; i <= test_chan_end; i++)
        {
            printf("%05d ", adc_codes[i]);
        }
        printf("\nVolts: ");
        for (unsigned int i = test_chan_start; i <= test_chan_end; i++)
        {
            printf("%.3f ", adc_volts[i]);
        }
        printf("\n");
    }

    // Check limits
    int test_result = 0; // 0=SUCCESS, 1=FAILED
    for (unsigned int i = test_chan_start; i <= test_chan_end; i++)
    {
        if ((adc_volts[i] > vlim_high) || (adc_volts[i] < vlim_low))
        {
            printf("FAIL: Ch.%d In=%.3f VL=%.3f VH=%.3f\n", i , adc_volts[i], vlim_low, vlim_high);
            test_result = 1;
        }
        else
            printf("PASS: Ch.%d\n", i);
    }

    // Close device
    if (verbose_enabled)
        printf("Closing device...\n");

    ioctl(fd, DMMAT_STOP, 0);
    free(p_sample);
    close(fd);

    if (test_result != 0)
    {
        printf("Test FAILED\n");
        exit(1);
    }
    else
    {
        printf("Test SUCCESS\n");
    }

    return test_result;
}
