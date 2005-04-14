/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************
  
  Register offsets and settings for WinSystems PCM-COM8 serial board

*/

#ifndef WIN_COM8_H
#define WIN_COM8_H

/* Board configuration registers */
#define COM8_BC_IR	0	/* Index register, R/W */
#define COM8_BC_BAR	1	/* Base address register, R/W */
#define COM8_BC_IAR	2	/* IRQ assignment register, R/W */
#define COM8_BC_IIR	3	/* Interrupt ID register, RO */
#define COM8_BC_ECR	4	/* EEPROM command register, R/W */
#define COM8_BC_EHDR	5	/* EEPROM high data register, R/W */
#define COM8_BC_ELDR	6	/* EEPROM low data register, R/W */
#define COM8_BC_CR	7	/* Command register, WO */
#define COM8_BC_SR	7	/* Status register, RO */

/* settings */
#define COM8_BC_UART_ENABLE	0x80	/* setting in BAR to enable uart */

/* Interrupt id register values (ISR) */
#define WIN_COM8_IIR_RDTO	0x0c	/* receive data timeout */
#define WIN_COM8_IIR_RSC	0x10	/* received xoff/special character */
#define WIN_COM8_IIR_CTSRTS	0x20	/* CTS/RTS change */

#endif
