//#define DEBUG
/*
FilterTranslate.c

This translates Systolix AD7725 Filter configuration (.cfg) files into an array
of unsigned shorts to be sent to the a2d_driver.

The first short is the number of short data values following.
The last short is the checksum (it is rumored.

Usage:

ftrans <Systolix .cfg filename> <output filename>

Note:  The DSM code expects the filename "filtercoeff.bin" to reside in /tmp

Original author:	Grant Gray

Copyright National Center for Atmospheric Research

*/

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	FILE *fpin, *fpout;
	char inny[10];
	unsigned short outy[2048], *pouty, csum = 0;
	int i = 0;

	if(argc != 3)
	{
		printf("Usage:\n");
		printf("ftrans <Systolix .cfg filename> <output filename>\n");
		exit(-1);
	}

	if((fpin = fopen(argv[1], "r")) == NULL)
	{
		printf("Cannot open %s for reading \n", argv[1]);
		exit(-1);
	}	

	if((fpout = fopen(argv[2], "wb")) == NULL)
	{
		printf("Cannot open %s for writing\n", argv[2]);
		exit(-1);
	}

	printf("Starting into the file\n");
	pouty = outy;
	pouty++;		//Reserve one for the count

	while(!feof(fpin))
	{
		fgets(inny, 10, fpin);
		sscanf(inny, "%04x\n", pouty);
#ifdef DEBUG
		printf("I %04d, A 0x%08x, D 0x%04x\n", i, pouty, *pouty);
#endif	
		csum += *pouty;
		pouty++;
		i++;
	}
	outy[0] = i-1;	
	fwrite(outy, 2, i, fpout);

	fclose(fpin);
	fclose(fpout);	
	printf("Wrote %d bytes, Checksum = 0x%04X, last value = 0x%04X\n",
		i*2, csum, pouty[i-2]);
}

