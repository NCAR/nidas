/* MessageDefs.h
   Message packet defines.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef MESSAGEDEFS_H
#define MESSAGEDEFS_H

#include <header.h>

#define DSM_MSG_MAX_LEN     	160     /* max message length		*/
#define DSM_MSG_STRING_SIZE     80      /* max len of msg string	*/

/* Message types. */
#define ANALOG_MSG	0		/* analog control message	*/
#define DATE_MSG	1		/* date set message		*/
#define FLIGHT_MSG	2		/* flight number message	*/
#define MCR_MSG  	3               /* mcr message			*/
#define NET_MSG  	4               /* network message		*/
#define PMS1_MSG 	5               /* pms 1d message		*/
#define PMS2_MSG  	6               /* pms 2d message		*/
#define STATUS_MSG  	7               /* status message		*/
#define TAPE_MSG  	8               /* tape message			*/
#define TASALT_MSG 	9               /* true air speed and pressure alt */
#define TIME_MSG 	10              /* time set message		*/
#define LOG_MSG 	11              /* Disk log message		*/
#define DIGOUT_MSG 	12              /* GenericButtons, DigOut	*/
#define MASP_MSG  	13              /* Masp message			*/
#define DSMTIME_MSG  	14              /* Individ DSM time to discWin	*/
#define DT_MSG  	15              /* Date and time when no GPS    */ 

/* Analog control actions */
#define ANALOG_RUN_MODE		0	/* select normal analog run mode */
#define ANALOG_CAL_MODE		1	/* select analog auto-cal mode	*/
#define ANALOG_QC_MODE		2	/* select analog quality check mode */
#define ANALOG_DIAG_MODE	3	/* select analog diagnostics mode */
#define ANALOG_REPORT_MODE	4	/* select analog report mode	*/

/* Analog control parameter values */
#define ANALOG_V10_SELECT       0       /* select 10 volt analog cal voltage */
#define ANALOG_V5_SELECT        1       /* select 5 volt analog cal voltage */
#define ANALOG_V2D5_SELECT      2       /* select 2.5 volt analog cal voltage */
#define ANALOG_V1_SELECT        3       /* select 1 volt analog cal voltage */
#define ANALOG_V0_SELECT        4       /* select 0 volt analog cal voltage */
#define ANALOG_V10N_SELECT      5       /* select -10 volt analog cal voltage */

#define ANALOG_ALL_SELECT	-2	/* select all analog channels */
 
#define ANALOG_GAIN_1_SELECT	1	/* select channel gain of 1 */
#define ANALOG_GAIN_1B_SELECT	-1	/* select channel gain of 1 */
#define ANALOG_GAIN_2_SELECT	2	/* select channel gain of 2 */
#define ANALOG_GAIN_4_SELECT	4	/* select channel gain of 4 */
#define ANALOG_GAIN_20_SELECT	20	/* select channel gain of 20 */

#define ANALOG_OFFSET_0_SELECT	0	/* select channel offset of 0 */
#define ANALOG_OFFSET_10_SELECT	10	/* select channel offset of 10 */

/* Tape message actions. */
#define TAPE_RECORD		0	/* start recording */
#define TAPE_STOP		1	/* stop recording */
#define TAPE_UNLOAD		2	/* unload the drive */

/* Mcr message actions */
#define MCR_REMOTE_ENABLE	0	/* dsm control enable/disable */
#define MCR_PREAMP_ENABLE	1	/* preamp power control/status */
#define MCR_DOOR_ENABLE		2	/* door power control/status */
#define MCR_DOOR_SELECT		3	/* door operation control/status */
#define MCR_HEATER_SELECT	4	/* heater operation control/status */
#define MCR_STATUS		5	/* status change message */

/* Masp message actions */
#define MASP_SET_PARAM		0	/* masp param_map */

/* Net message actions */
#define NET_STATUS		0	/* network status message */
#define NET_CONNECTED		1	/* network is connected */
#define NET_DISCONNECTED	2	/* network is disconnected */
#define NET_ENABLE		3	/* enable network data flow */
#define NET_SUSPEND		4	/* suspend network data flow */

/* Pms2d message actions. */
#define PMS2_STATUS		0	/* pms 2d status message */
#define PMS2_TAS_SELECT		1	/* tas selection message */
#define PMS2_RATE_SELECT	2	/* record rate selection message */
#define PMS2_PROBE_SELECT	3	/* probe selection message */
 
#endif
