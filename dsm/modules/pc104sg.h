/* Pc104sg.h
   Class for interfacing the PC104-SG time and frequency processor.

   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research

   Revisions:

*/

#ifndef PC104SG_H
#define PC104SG_H

/* clock status definition for user space code: same as extended status bits */
#define CLOCK_STATUS_NOSYNC       0x01 /* Set if NOT in sync */
#define CLOCK_STATUS_NOCODE       0x02 /* Set if selected input code NOT decodeable */
#define CLOCK_STATUS_NOPPS        0x04 /* Set if PPS input invalid */
#define CLOCK_STATUS_NOMAJT       0x08 /* Set if major time NOT set since jam */
#define CLOCK_STATUS_NOYEAR       0x10 /* Set if year NOT set */

#ifdef __RTCORE_KERNEL__

#define PC104SG_IOPORT_WIDTH  0x20

/* register definitions for PC104-SG card */
#define Status_Port              0x01 /* status & interrupt enables */
#define Ext_Ready                1    /* Ext. Time Tag Data Ready if 1*/
#define Sync_OK                  2    /* In-sync to time reference if 1 */
#define RAM_FIFO_Ready           4    /* Ram_Fifo_Port byte Ready if 1 */
#define Match                    8    /* Match register toggles on Match */
#define Heartbeat                0x10 /* Heartbeat pulse detected if 1 */
#define Heartbeat_Int_Enb        0x20 /* Heartbeat interrupt enabled if 1 */
#define Match_Int_Enb            0x40 /* Match interrupt enabled if 1 */
#define Ext_Ready_Int_Enb        0x80 /* Ext. Time Tag intr. enabled if 1 */

#define Dual_Port_Data_Port      0x02 /* Write commands or dual port data */

#define Extended_Status_Port     0x03
#define Command_Full             0x01 /* READ ONLY Command Port not ready */
#define Response_Ready           0x02 /* READ ONLY Command DP response */
#define External_Polarity        0x04 /* R/W 0=POS EDGE 1=NEG EDGE */
#define Extended_Match_Enable    0x08 /* R/W 1=enabled. -XM OPTION ONLY*/

#define Reset_Port               0x01 /* WRITE ONLY */
#define Assert_Board_Reset       0x1e /* WR bit0 = 0: Assert cold start */
#define Release_Board_Reset      0x1d /* WR bit1 = 0: Release cold start */
#define Trigger_Sim_Ext_Time_Tag 0x1b /* WR bit2 = 0: Fake ext. time tag */
#define Reset_Match              0x17 /* WR bit3 = 0: Reset Match reg */
#define Reset_Heartbeat          0x0f /* WR bit4 = 0: Reset Heartbeat flag*/

#define Match_Usec10_Usec1_Port  0x05 /* WRITE only 10e1,10e0 match usec */

#define Dual_Port_Address_Port   0x06 /* WRITE only dual port address */

#define Match_Msec1_Usec100_Port 0x07 /* WRITE only 10e3,10e2 match usec */

#define Usec1_Nsec100_Port       0x0f /* READ use0,nse2 and latch new time */
#define Usec100_Usec10_Port      0x0e /* READ latched usec e2,usec e1 */
#define Msec10_Msec1_Port        0x0d /* READ latched msec e1,msec e1 */
#define Sec1_Msec100_Port        0x0c /* READ latched sec e0, msec e3 */
#define Min1_Sec10_Port          0x0b /* READ latched min e0, sec e1 */
#define Hr1_Min10_Port           0x0a /* READ latched hour e0, min e1 */

#define Day1_Hr10_Port           0x09 /* READ latched day e0, hr e1 */
#define Day100_Day10_Port        0x08 /* READ latched day e2, day e1 */
#define Year10_Year1_Port        0x07 /* READ latched year e1, year e0 */
/* READING ANY OF THE FOLLOWING PORTS WILL CLEAR EXT READY FLAG */
#define Ext_Usec1_Nsec100_Port   0x1f /* READ ext. time tag usec e0, ns e2 */
#define Ext_Usec100_Usec10_Port  0x1e /* READ ext. time tag usec e2, us e1 */
#define Ext_Msec10_Msec1_Port    0x1d /* READ ext. time tag msec e1, msec e0*/
#define Ext_Sec1_Msec100_Port    0x1c /* READ ext. time tag sec e0, msec e2 */
#define Ext_Min1_Sec10_Port      0x1b /* READ ext. time tag min e0, sec e1 */
#define Ext_Hr1_Min10_Port       0x1a /* READ ext. time tag hour e0, min e1 */
#define Ext_Day1_Hr10_Port       0x19 /* READ ext. time tag day e0, hour e1 */
#define Ext_Day100_Day10_Port    0x18 /* READ ext. time tag day e2, day e1 */
#define Ext_Year10_Year1_Port    0x17 /* READ ext. time tag year e1, ns e2 */

#define DP_Command 0xff
#define No_op                       0 /* no operation */
#define Command_Set_Major           2 /* Set clock seconds..days to Major seconds..days */

#define Command_Set_Years           4 /* Set clock years to dual port years  */
#define Command_Set_RAM_FIFO        6 /* Set RAM FIFO external time tag mode */
#define Command_Reset_RAM_FIFO      8 /* Reset RAM FIFO external time tag mode */
#define Command_Empty_RAM_FIFO     10 /* Empty RAM FIFO */
#define Command_Set_Ctr0           12 /* Set 82C54 ctr 2 ("lowrate") params */
#define Command_Set_Ctr1           14 /* Set 82C54 ctr 2 ("heartbeat") params */
#define Command_Set_Ctr2           16 /* Set 82C54 ctr 2 ("rate2") params */
#define Command_Rejam              18 /* Re-jam at start of next second */

/*   DUAL PORT RAM LOCATIONS 0x00..0x7F ARE READ-ONLY */
#define DP_Extd_Sts              0x00 /* Extended status READ ONLY */
#define DP_Extd_Sts_Nosync       0x01 /* Set if NOT in sync */
#define DP_Extd_Sts_Nocode       0x02 /* Set if selected input code NOT decodeable */
#define DP_Extd_Sts_NoPPS        0x04 /* Set if PPS input invalid */
#define DP_Extd_Sts_NoMajT       0x08 /* Set if major time NOT set since jam */
#define DP_Extd_Sts_NoYear       0x10 /* Set if year NOT set */
#define DP_Code_CtlA             0x0f /* Control field "A" read data */
#define DP_Code_CtlB             0x10 /* Control field "B" read data */
#define DP_Code_CtlC             0x11 /* Control field "C" read data */
#define DP_Code_CtlD             0x12 /* Control field "D" read data */
#define DP_Code_CtlE             0x13 /* Control field "E" read data */
#define DP_Code_CtlF             0x14 /* Control field "F" read data */
#define DP_Code_CtlG             0x15 /* Control field "G" read data */


#define DP_Control0              0x80 /* Dual Port Ram Address for Control Register */
#define DP_Control0_Leapyear     0x01 /* Current year is leap year*/
#define DP_Control0_CodePriority 0x02 /* Time code input has priority over PPS*/
#define DP_Control0_NegCodePropD 0x04 /* code input prop delay setting is - */
#define DP_Control0_NegPPSPropD  0x08 /* PPS input prop delay setting is - */

#define DP_CodeSelect            0x81 /* Time code input select */
#define DP_CodeSelect_IRIGB      0x0b /* IRIG-B */
#define DP_CodeSelect_IRIGA      0x0a /* IRIG-A */
#define DP_CodeSelect_NASA36     0x06 /* NASA36 */
#define DP_CodeSelect_2137       0x07 /* 2137   */
#define DP_CodeSelect_XR3        0x03 /* XR3    */
#define DP_CodeSelect_IRIGG      0x0f /* IRIG-G */
#define DP_CodeSelect_IRIGE      0x0e /* IRIG-E */

#define DP_LeapSec_Day10Day1     0x82 /* Day (10s & 1s) ending in leap sec*/

#define DP_LeapSec_Day1000Day100 0x83 /* Day (0,100s) ending in leap sec*/

#define DP_CodePropDel_ns100ns10 0x84  /* time code prop. delay 100,10 ns */
#define DP_CodePropDel_us10us1   0x85  /* time code prop. delay 10,1 us */
#define DP_CodePropDel_ms1us100  0x86  /* time code prop. delay 1000,100 us */
#define DP_CodePropDel_ms100ms10 0x87  /* time code prop. delay 100,10 ms */


#define DP_PPS_PropDel_ns100ns10 0x88       /* PPS prop. delay 100,10 ns  */
#define DP_PPS_PropDel_us10us1   0x89       /* PPS prop. delay 10,1 us    */
#define DP_PPS_PropDel_ms1us100  0x8a       /* PPS prop. delay 1000,100 us*/
#define DP_PPS_PropDel_ms100ms10 0x8b       /* PPS prop. delay 100,10 ms  */

#define DP_PPS_Time_ns100ns10    0x8c          /* PPS time 100,10ns */
#define DP_PPS_Time_us10us1      0x8d          /* PPS time 10,1us */
#define DP_PPS_Time_ms1us100     0x8e          /* PPS time 1000,100us */

#define DP_PPS_Time_ms100ms10    0x8f          /* PPS time 100,10ms */
#define DP_Major_Time_s10s1      0x90          /* Major time 10,1second */
#define DP_Major_Time_m10m1      0x91          /* Major time 10,1minute */
#define DP_Major_Time_h10h1      0x92          /* Major time 10,1hour */
#define DP_Major_Time_d10d1      0x93          /* Major time 10,1 day */
#define DP_Major_Time_d100       0x94          /* Major time 100 day */
#define DP_Major_Time_d100d10    0x94          /* Major time 100 day */
#define DP_Year10_Year1      0x95               /* 10,1 years */
#define DP_Year1000_Year100  0x96               /* 1000,100 years */
#define DP_Codebypass        0X97               /* #frames to validate code */
#define DP_Ctr2_ctl          0X98     /* ctr 2 control word */
#define DP_Ctr1_ctl          0x99     /* ctr 1 control word */
#define DP_Ctr0_ctl          0x9A     /* ctr 0 control word */
#define DP_Ctr2_ctl_sel                 0x80  /* ALWAYS used for ctr 2 */
#define DP_Ctr1_ctl_sel                 0x40  /* ALWAYS used for ctr 1 */
#define DP_Ctr0_ctl_sel                 0x00  /* ALWAYS used for ctr 0 */
#define DP_ctl_rw                       0x30  /* ALWAYS used */
#define DP_ctl_mode0                    0x00  /* Ctrx mode 0 select */
#define DP_ctl_mode1                    0x02  /* Ctrx mode 1 select */
#define DP_ctl_mode2                    0x04  /* Ctrx mode 2 select */
#define DP_ctl_mode3                    0x06  /* Ctrx mode 3 select */
#define DP_ctl_mode4                    0x08  /* Ctrx mode 4 select */
#define DP_ctl_mode5                    0x0A  /* Ctrx mode 5 select */
#define DP_ctl_bin                      0x00  /* Ctrx binary mode select */
#define DP_ctl_bcd                      0x01  /* Ctrx bcd mode select */
#define DP_Ctr2_lsb          0x9B     /* ctr 2 count LSB    */
#define DP_Ctr2_msb          0x9C     /* ctr 2 count MSB    */
#define DP_Ctr1_lsb          0x9D     /* ctr 1 count LSB    */
#define DP_Ctr1_msb          0x9E     /* ctr 1 count MSB    */
#define DP_Ctr0_lsb          0x9F     /* ctr 0 count LSB    */
#define DP_Ctr0_msb          0xA0     /* ctr 0 count MSB    */
#define DP_Sync_Timeout      0xA1     /* sync OK time out (BCD,mult of .2 sec) */
#define DP_Sync_Thresh       0xA2     /* sync OK threshold (BCD,mult of .1usec */
#define DP_HQ_TFOM           0xB2     /* Have-Quick TFOM. Used for -HQ option */
#define DP_BATTIM_Holdoff_m10m1 0xBD  /* BATTIM wait (minutes) to set Major Time */
#define DP_BATTIM_Jam_s10s1  0xC4     /* BATTIM wait (seconds) to copy bat time
                                         to clock w/o 1pps */
#define DP_Match_ht          0xBE     /* match time .ht (BCD)  */
#define DP_Match_SS          0xBF     /* match time ss (BCD)  */
#define DP_Match_MM          0xCB     /* match time mm (BCD)  */
#define DP_Match_HH          0xCC     /* match time hh (BCD)  */
#define DP_Match_D1D0        0xCD     /* match time day (10,1 digits) (BCD) */
#define DP_Match_XD2         0xCE     /* match time don't care, 100 days  */

#define PC104SG_CALENDAR_SIZE     13              // length of calendar array
#define PC104SG_LEAP_YEAR_MOD     4               // leap year modulus

#define BCD_2_DIGIT_MASK        0x00FF          // mask to read 2 bcd digits
#define BCD_DIGIT_SHIFT         4               // bits to shift 1 bcd digits
#define BCD_2_DIGIT_SHIFT       8               // bits to shift 2 bcd digits
#define BCD_3_DIGIT_SHIFT       12              // bits to shift 3 bcd digits
#define BCD_4_DIGIT_SHIFT       16              // bits to shift 4 bcd digits

#endif	/* __RTCORE_KERNEL__ */

#endif
