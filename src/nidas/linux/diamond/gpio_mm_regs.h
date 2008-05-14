/*
   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Defines for register and register settings on Diamond Systems
   GPIO-MM Digital I/O Cards.

   Original Author: Gordon Maclean

   Copyright 2008 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef NIDAS_DIAMOND_GPIO_MM_REGS_H
#define NIDAS_DIAMOND_GPIO_MM_REGS_H

/* How many counters on the board */
#define GPIO_MM_CNTR_PER_BOARD 10

/* How many counters on a 9513 chip */
#define GPIO_MM_CNTR_PER_CHIP 5

/* defines for GPIO-MM board clock */
#define GPIO_MM_9513_1_DATA         0x00
#define GPIO_MM_9513_1_PTR          0x01
#define GPIO_MM_DIO                 0x02
#define GPIO_MM_9513_2_DATA         0x04
#define GPIO_MM_9513_2_PTR          0x05
#define GPIO_MM_IRQ_CTRL            0x06
#define GPIO_MM_EEPROM_DATA         0x08
#define GPIO_MM_EEPROM_ADDR         0x09
#define GPIO_MM_EEPROM_CTRL         0x0A
#define GPIO_MM_FPGA_REV            0x0B
#define GPIO_MM_IRQ_SRC             0x0C
#define GPIO_MM_IRQ_CTL_STATUS      0x0D
#define GPIO_MM_AUX_DIO             0x0E
#define GPIO_MM_RESET_ID            0x0F

/* 9513 data register for chip x, x=0,1 */
#define GPIO_MM_9513_DATA(x) ((x)*4)
/* 9513 pointer register for chip x, x=0,1 */
#define GPIO_MM_9513_PTR(x) ((x)*4+1)

#define GPIO_MM_8255_1_DIO_A        0x00
#define GPIO_MM_8255_1_DIO_B        0x01
#define GPIO_MM_8255_1_DIO_C        0x02
#define GPIO_MM_8255_1_CTRL         0x03
#define GPIO_MM_8255_2_DIO_A        0x04
#define GPIO_MM_8255_2_DIO_B        0x05
#define GPIO_MM_8255_2_DIO_C        0x06
#define GPIO_MM_8255_2_CTRL         0x07

#define CTS9513_DPTR_CNTR1_MODE   0x01
#define CTS9513_DPTR_CNTR2_MODE   0x02
#define CTS9513_DPTR_CNTR3_MODE   0x03
#define CTS9513_DPTR_CNTR4_MODE   0x04
#define CTS9513_DPTR_CNTR5_MODE   0x05

#define CTS9513_DPTR_ALARM_1      0x07

#define CTS9513_DPTR_CNTR1_LOAD   0x09
#define CTS9513_DPTR_CNTR2_LOAD   0x0A
#define CTS9513_DPTR_CNTR3_LOAD   0x0B
#define CTS9513_DPTR_CNTR4_LOAD   0x0C
#define CTS9513_DPTR_CNTR5_LOAD   0x0D

#define CTS9513_DPTR_ALARM_2      0x0F

#define CTS9513_DPTR_CNTR1_HOLD   0x11
#define CTS9513_DPTR_CNTR2_HOLD   0x12
#define CTS9513_DPTR_CNTR3_HOLD   0x13
#define CTS9513_DPTR_CNTR4_HOLD   0x14
#define CTS9513_DPTR_CNTR5_HOLD   0x15

#define CTS9513_DPTR_MASTER_MODE  0x17

#define CTS9513_DPTR_HOLD_CYCL1   0x19
#define CTS9513_DPTR_HOLD_CYCL2   0x1A
#define CTS9513_DPTR_HOLD_CYCL3   0x1B
#define CTS9513_DPTR_HOLD_CYCL4   0x1C
#define CTS9513_DPTR_HOLD_CYCL5   0x1D

#define CTS9513_DPTR_STATUS       0x1F

#define CTS9513_ARM               0x20
#define CTS9513_LOAD              0x40
#define CTS9513_LOAD_ARM          0x60
#define CTS9513_DISARM_SAVE       0x80
#define CTS9513_SAVE_TO_HOLD      0xA0
#define CTS9513_DISARM            0xC0

#define CTS9513_CLEAR_TOGGLE_OUT  0xE0
#define CTS9513_SET_TOGGLE_OUT    0xE8
#define CTS9513_STEP              0xF0

/* master mode register */
#define CTS9513_MML_NO_TOD         0x00  /* time-of-day disabled */
#define CTS9513_MML_TOD5           0x01  /* time-of-day enabled, / 5 */
#define CTS9513_MML_TOD6           0x02  /* time-of-day enabled, / 6 */ 
#define CTS9513_MML_TOD10          0x03  /* time-of-day enabled, / 10 */

#define CTS9513_MML_NO_COMP        0x00  /* comparators disabled */
#define CTS9513_MML_COMP1          0x04  /* comparator 1 on */
#define CTS9513_MML_COMP2          0x08  /* comparator 2 on */
#define CTS9513_MML_COMP12         0x0C  /* comparator 1 & 2 on */

#define CTS9513_MML_FOUT_F1X       0x00  /* FOUT source = F1 */
#define CTS9513_MML_FOUT_S1        0x10  /* FOUT source = source 1 */
#define CTS9513_MML_FOUT_S2        0x20
#define CTS9513_MML_FOUT_S3        0x30
#define CTS9513_MML_FOUT_S4        0x40
#define CTS9513_MML_FOUT_S5        0x50
#define CTS9513_MML_FOUT_G1        0x60  /* FOUT source = gate 1 */
#define CTS9513_MML_FOUT_G2        0x70
#define CTS9513_MML_FOUT_G3        0x80
#define CTS9513_MML_FOUT_G4        0x90
#define CTS9513_MML_FOUT_G5        0xA0
#define CTS9513_MML_FOUT_F1        0xB0  /* FOUT source = F1 */
#define CTS9513_MML_FOUT_F2        0xC0
#define CTS9513_MML_FOUT_F3        0xD0
#define CTS9513_MML_FOUT_F4        0xE0
#define CTS9513_MML_FOUT_F5        0xF0

#define CTS9513_MMH_FOUT_DIV16     0x00
#define CTS9513_MMH_FOUT_DIV1      0x01
#define CTS9513_MMH_FOUT_DIV2      0x02
#define CTS9513_MMH_FOUT_DIV3      0x03
#define CTS9513_MMH_FOUT_DIV4      0x04
#define CTS9513_MMH_FOUT_DIV5      0x05

#define CTS9513_MMH_FOUT_ON        0x00
#define CTS9513_MMH_FOUT_OFF       0x10

#define CTS9513_MMH_WIDTH_8        0x00
#define CTS9513_MMH_WIDTH_16       0x20

#define CTS9513_MMH_DP_ENABLE      0x00
#define CTS9513_MMH_DP_DISABLE     0x40

#define CTS9513_MMH_BIN            0x00
#define CTS9513_MMH_BCD            0x80

/* Counter mode definitions */
#define CTS9513_CML_OUT_LOW        0x00
#define CTS9513_CML_OUT_HIGH_ON_TC 0x01
#define CTS9513_CML_OUT_TC_TOGGLE  0x02
#define CTS9513_CML_OUT_HIGH       0x04
#define CTS9513_CML_OUT_LOW_ON_TC  0x05

#define CTS9513_CML_CNT_DN         0x00
#define CTS9513_CML_CNT_UP         0x08

#define CTS9513_CML_CNT_BIN        0x00
#define CTS9513_CML_CNT_BCD        0x10

#define CTS9513_CML_ONCE           0x00
#define CTS9513_CML_REPEAT         0x20

#define CTS9513_CML_RELOAD_LOAD    0x00     /* on TC, reload from load reg */
#define CTS9513_CML_RELOAD_BOTH    0x40     /* on TC, reload based on gate */

#define CTS9513_CML_GATE_NORETRIG 0x00
#define CTS9513_CML_GATE_RETRIG    0x80

#define CTS9513_CMH_SRC_TCNM1      0x00
#define CTS9513_CMH_SRC_S1         0x01
#define CTS9513_CMH_SRC_S2         0x02
#define CTS9513_CMH_SRC_S3         0x03
#define CTS9513_CMH_SRC_S4         0x04
#define CTS9513_CMH_SRC_S5         0x05
#define CTS9513_CMH_SRC_G1         0x06
#define CTS9513_CMH_SRC_G2         0x07
#define CTS9513_CMH_SRC_G3         0x08
#define CTS9513_CMH_SRC_G4         0x09
#define CTS9513_CMH_SRC_G5         0x0A
#define CTS9513_CMH_SRC_F1         0x0B
#define CTS9513_CMH_SRC_F2         0x0C
#define CTS9513_CMH_SRC_F3         0x0D
#define CTS9513_CMH_SRC_F4         0x0E
#define CTS9513_CMH_SRC_F5         0x0F

#define CTS9513_CMH_EDGE_RISING    0x00
#define CTS9513_CMH_EDGE_FALLING   0x10

#define CTS9513_CMH_NO_GATE        0x00
#define CTS9513_CMH_GATE_HI_TCNM1  0x20
#define CTS9513_CMH_GATE_HI_GNP1   0x40
#define CTS9513_CMH_GATE_HI_GNM1   0x60
#define CTS9513_CMH_GATE_HI_GN     0x80
#define CTS9513_CMH_GATE_LO_GN     0xA0
#define CTS9513_CMH_GATE_HI_EDGE_GN 0xC0
#define CTS9513_CMH_GATE_LO_EDGE_GN 0xE0

#endif
