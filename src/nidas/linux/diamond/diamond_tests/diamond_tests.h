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

#endif
