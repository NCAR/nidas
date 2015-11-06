/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
//#define DEBUG
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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
/*
FilterTranslate.c

This translates Systolix AD7725 Filter configuration files into an array
of unsigned shorts to be sent to the a2d_driver.

The first short is the number of short data values following.

Original author:	Grant Gray

*/

#define FILTBLOCK	12
#define FILTBLLEN	43

#include <stdio.h>

main(int argc, char **argv)
{
	FILE *fpin, *fpout;
	char inny[10];
	unsigned short outy[2048], *pouty;
	int i = 0, j, k;

	if(argc != 3)
	{
		printf("Invalid argument count\n");
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
	i++;

	fgets(inny, 10, fpin);		// Convert an ascii hex number
	sscanf(inny, "%04x\n", pouty);
	pouty++;			// Place converted number in out buffer
	i++;				// Increment the counter

	// Do the above for the 12 data blocks of 42 16 bit workds each + CRC(16)
	for(j = 0; j < FILTBLOCK; j++)
	{
		for(k = 0; k < FILTBLLEN; k++)
		{
		fgets(inny, 10, fpin);
		sscanf(inny, "%04x\n", pouty);
		pouty++;
		i++;
		}
	}
	outy[0] = i-1;	
	fwrite(outy, 2, i, fpout);

	fclose(fpin);
	fclose(fpout);	

	printf("Wrote %d bytes, last value = 0x%04X\n",
		i*2, pouty[i-2]);

	for(j = 0; j < 1 + (FILTBLOCK*FILTBLLEN + 2)/8; j++)
	{
		printf("%04X:", j*16); // Print byte address offset
		for(k = 0; k < 8; k++)
		{
			printf(" %04x", outy[j*8+k]);
		}

		printf("\n");
	}
	printf("\n");
}


