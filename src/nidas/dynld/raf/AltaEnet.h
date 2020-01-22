

#ifndef _alta_enet_h_
#define _alta_enet_h_

#include <stdint.h>

/**
 * Alta APMP packet header.  Data will all be big endian.
 */
typedef struct
{
  uint32_t	mode;		// Should always be 1 for APMP mode.
  uint32_t	seqNum;
  uint32_t	status;		// Value of zero is success.
  uint32_t	alta;		// Fixed string "ALTA" - 0x414C5441
  uint32_t	reserved1;
  uint32_t	reserved2;
  uint32_t	reserved3;
  uint32_t	payloadSize;	// in bytes; 4-1376

// These are not really part of the header, but are standard first part of the
// payload for the APMP UDP feed
  uint32_t	PEtimeHigh;
  uint32_t	PEtimeLow;
  uint32_t	IRIGtimeHigh;	// Julian Day (Year if available).
  uint32_t	IRIGtimeLow;	// BCD hour, minute, second.
} APMP_hdr;


// Alta receive packet for each arinc word.
typedef struct
{
  uint32_t	control;	// 0x80000000 is error bit (0=ok, 1=err)
  uint32_t	timeHigh;
  uint32_t	timeLow;	// 20ns / 50Mhz interval timer.
  uint32_t	data;
} rxp;


// Repackaged RXP to pass from UDPARincSensor to DSMArincSensor.
typedef struct
{
  uint32_t time;	// Milliseconde since midnight.
  uint32_t data;	// Arinc 429 word.
} txp;


#endif
