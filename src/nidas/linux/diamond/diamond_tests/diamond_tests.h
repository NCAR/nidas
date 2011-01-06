#ifndef DIAMOND_TESTS_H
#define DIAMOND_TESTS_H

#include <nidas/linux/diamond/dmd_mmat.h>

struct cmd_options {
	int total_set;
	unsigned char mode;
	unsigned char smode;
} flags = {0, 0, 0};

struct D2APt {
	unsigned int value;
	unsigned int channel;
};

struct dmd_command {
	unsigned addr;
	unsigned char value;
};

struct waveform{
	int channel;
	int size;
	int point[0];
};

struct D2D_Config{
	int num_chan_out;
	int waveform_scan_rate;
};

#endif
