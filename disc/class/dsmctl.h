/* dsmctl.h
   dsmctl specific defines.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef DSMCTL_H
#define DSMCTL_H

#include <header.h>
//#include <socket.h>

#ifndef FALSE
#define FALSE    0
#endif

#ifndef TRUE
#define TRUE     1
#endif

#ifndef ERROR
#define ERROR   -1
#endif

#ifndef OK
#define OK	 0
#endif
#define TOG	 2			/* toggling buffer size */

#define FEET_PER_METER       	3.28084 /* meters to feet conversion */
#define METERS_PER_NAUT		1852.0	/* meters per nautical mile */
#define SECS_PER_MIN         	60      /* minutes to seconds conversion */
#define SECS_PER_HOUR         	3600    /* minutes to seconds conversion */

#define MAX_DESC_SIZE		128	/* max header descriptor size, bytes */
#define MAX_DSM			8	/* max number of dsms */
#define MAX_SDI_RATES   	4	/* max number of sdi sample rates */
#define MAX_CMIG_INTFC		2	/* max num cmigits3 irs intfc per dsm*/
#define MAX_HWIRS_INTFC		2	/* max num honeywell irs intfc per dsm*/
#define MAX_HWGPS_INTFC		2	/* max num honeywell gps intfc per dsm*/
#define MAX_GPS_INTFC           2       /* max num ARINC GPS per dsm */
#define MAX_GREYVME_INTFC	2	/* max num vme grey scale intfc */
#define MAX_OPHIR3_INTFC        2       /* max num Ophir3 radiometers per dsm*/
#define MAX_PMSVME2D_INTFC	2	/* max num vme pms 2d intfc per dsm */
#define MAX_PMS1VME_INTFC	2	/* max num vme pms 1d intfc per dsm */
#define MAX_SER_INTFC		8	/* max num serial interfaces per dsm */
#define MAX_SPP_INTFC		3	/* max num SPP interfaces per dsm */
#define MAX_TANS_INTFC          2       /* max num of tans receivers per dsm */
#define MAX_PPS_INTFC		1       /* max num of PPS receivers per dsm */
#define MAX_UVHYG_INTFC         2       /* max num of UV hygrometers per dsm */
#define MAX_VMIO12_INTFC        2       /* max num of VMIO12 dig IO boards */
#define MAX_SERIALTOD_INTFC     8       /* max num of SerialTod ports */

/* Define the isio port numbers. Port numbers begin at 1. */
#define SERTOD_PORT		1	/* Serial TOD output */
#define AEROSOL_PORT_1          2       /* NOAA AEROSOL #1 */
#define MASSPEC_PORT_1		3	/* Mass Spec # 1 */
#define NATS_PORT		2	/* telemetry communication port */
#define UVHYG_PORT_1          	6      	/* uv hygrometer #1 */
#define UVHYG_PORT_2          	5      	/* uv hygrometer #2 */
#define TANS_PORT_1          	8       /* Trimble tans #1 */
#define TANS_PORT_2          	6       /* Trimble tans #2 */
#define GARMIN_PORT_1		3	/* Garmin GPS #1*/
#define GARMIN_PORT_2		6	/* Garmin GPS #2*/
#define CMIGITS3_PORT_1	        4	/* C-MIGITS 3 IRS #1*/
#define CMIGITS3_PORT_2		5	/* C-MIGITS 3 IRS #2*/
#define OPHIR_PORT_1          	7       /* Ophir radiometer port #1 */
#define OPHIR_PORT_2          	8       /* Ophir radiometer port #2 */
#define PPSGPS_PORT_1          	8       /* Collins PPS GPS port #1 */
#define JPLTDL_PORT_1           3       /* JPL TDL #1 */
#define LHTDL_PORT_1            4       /* LH TDL #1 */
#define LASAIR_PORT_1           6       /* LASAIR #1 */
#define CLIMET_PORT_1           6       /* CLIMET #1 */
#define MCA_PORT_1              7       /* MCA #1 */
#define NEPH_PORT_1             3       /* NEPH #1 */
#define NEPH_PORT_2             4       /* NEPH #1 */
#define RDMA_PORT_1             4       /* RDMA #1 */
#define NOAA_PORT_1             5       /* NOAA #1 */
#define NONOY_PORT_1            7       /* NO_NOY #1 */
#define DPRES_PORT_1            5       /* Digital Pressure #1 */
#define OZONE_PORT_1		4	/* OZONE #1 */
#define SPP_PORT_1		5	/* SPP 1D probe (pod) */
#define SPP_PORT_2		6	/* SPP 1D probe (pod) */
#define SPP_PORT_3		7	/* SPP 1D probe (pod) */

#define DSM_COMM_TAPE_PORT	1150	/* comm to tape task data socket port */
#define DSM_TAPE_WIN_PORT	1151	/* tape to win task data socket port */
#define DSM_COMM_WIN_PORT	1152	/* comm to win task data socket port */
#define DSM_BROADCAST_PORT	1153	/* comm to dsm broadcast socket port */

/* Winds message port is defined from $WINDS/include/socket.h. */
#define DSM_WINDS_PORT		(FIRST_AVAILABLE_PORT+GENERAL_LISTENING)
#define WINDS_BROADCAST_PORT	(FIRST_AVAILABLE_PORT+CALIB_NETDATA)

/* Drive states. */

#ifndef ADSCMD				/* necessary until old ads gone */

#define UNLOADED 	0               /* drive is unloaded */
#define LOADING  	1               /* drive is in process of loading */
#define LOADED   	2               /* drive is loaded */
#define RECORDING   	3               /* drive is recording */
#define UNLOADING  	4               /* drive is in process of unloading */
#define NOT_PRES 	5               /* drive is not present */
#define FAILED   	6               /* drive has failed */
#define BUSY     	7               /* drive is busy */
 
#define NDRV	 	1
#define DRV_0    	0               /* drive 0 identification */
#define DRV_NONE 	-1              /* no drive identification */
#define R1DRIVE         1               /* /jnet/r1 removable disk drive */
#define R2DRIVE         2               /* /jnet/r2 removable disk drive */

/* Pms 1d probe control words. */
#define RANGE_NONE      -2              /* non-ranging probe */
#define RANGE_0         0               /* pms 1d range 0 */
#define RANGE_1         1               /* pms 1d range 1 */
#define RANGE_2         2               /* pms 1d range 2 */
#define RANGE_3         3               /* pms 1d range 3 */
 
#define ASAS_OFF       	0     		/* asas off */
#define ASAS_ON        	1     		/* asas on */

/* Pms 2d defines. */
#define P1IDX      	0             	/* P1 probe index */
#define C1IDX      	1              	/* C1 probe index */
#define H1IDX      	1   		/* H1 probe idx, sampled as C1*/
#define P2IDX      	2        	/* P2 probe index */
#define C2IDX      	3         	/* C2 probe index */
#define H2IDX      	3        	/* H2 probe idx, sampled as C2*/

#define DISAB_2D   0x0000              	/* disable a probe */
#define ENAB_2D    0x00C2              	/* enable a probe */
#define INOP_2D    0x00FF              	/* probe not present */

#define TAS2D_DEFAULT   40              /* default tas setting */
#define TAS2D_AUTO   	0               /* auto tas setting */
 
#endif			/* ADSCMD */

#define RATE2D_OFF     	0               /* 2d recording off */
#define RATE2D_MAX 	100             /* max 2d recording rate */

/* Grey scale interface defines. */
#define GNONE           		0	/* no probe type */
#define GA1             		1	/* GA1 probe type */
#define GA2             		2	/* GA2 probe type */
#define GREY_NUM_PROBES			2	/* num probes per intfc */
#define GREY_DEFAULT_CHARGE_SPACING	5	/* mm per charge sample */
#define GREY_DEFAULT_CHARGE_LOCATION	0	/* charge ring offset, mm */

/* PMS 2D interface defines. */
#define PMS2D_NUM_PROBES		2	/* num probes per intfc */

/* IP429 arinc interface board defines. */
#define IP429_NUM_CHANS         6               // number of channels per board
#define IP429_NUM_RX_CHANS      4               // number of rx chans per board

/* MCR status word bit definitions. */
#define MCR_REMOTE_ACTIVE       0x0001	/* mcr detects dsm is active */
#define MCR_REMOTE_ENABLED      0x0002	/* dsm control of mcr enabled */
#define MCR_PREAMP_ON           0x0004	/* preamp is on */
#define MCR_PREAMP_OFF          0x0008	/* preamp is off */
#define MCR_RIGHT_DOOR_OPEN     0x0010	/* right door is open */
#define MCR_RIGHT_DOOR_CLOSED   0x0020	/* right door is closed */
#define MCR_LEFT_DOOR_OPEN      0x0100	/* left door is open */
#define MCR_LEFT_DOOR_CLOSED    0x0200	/* left door is closed */
#define MCR_HEATERS_ON          0x0400	/* heaters are on */
#define MCR_HEATERS_OFF         0x0800	/* heaters are off */

#define MCROUT_NAME     "MCROUT"        /* variable name for the mcr output */
#define MCRSTAT_NAME    "MCRSTAT"       /* var name for the mcr status input */
#define PSFD_NAME	"PSFD1"		/* var name for psfd */
#define PSFD2_NAME	"PSFD2"		/* var name for psfd2 */
#define RADAR_ALT_NAME	"HGM232"	/* var name for radar altimeter */

#endif
