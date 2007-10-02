/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    RTLinux serial driver for the ADS3 DSM.

    Much of this was taken from linux serial driver:
    	linux/drivers/char/serial.c

 ********************************************************************
*/

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_pthread.h>
#include <rtl_semaphore.h>
#include <rtl_core.h>
#include <rtl_unistd.h>
#include <rtl_time.h>
#include <rtl_posixio.h>

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/serial.h>
#include <linux/termios.h>
#include <linux/serial_reg.h>

#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/dsm_serial.h>
#include <nidas/rtlinux/dsm_viper.h>
#include <nidas/rtlinux/rtl_isa_irq.h>
#include <nidas/rtlinux/irigclock.h>
#include <nidas/rtlinux/win_com8.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/dsm_version.h>

// #define SERIAL_DEBUG_AUTOCONF
// #define DEBUG_ISR
// #define DEBUG

#ifndef MIN
#define MIN(a,b)     ((a) < (b) ? (a) : (b))
#endif

RTLINUX_MODULE(dsm_serial);

static char* devprefix = "dsmser";

/* Number of boards this module can support a one time */
#define MAX_NUM_BOARDS 3

/* Maximum number of ports on a board */
#define MAX_NUM_PORTS_PER_BOARD 8

/* how many serial cards are we supporting. Determined at init time
 * by checking known board types.
 */
static int numboards = 0;

/* type of board, from dsm_serial.h.  BOARD_UNKNOWN means board doesn't exist */
static int brdtype[MAX_NUM_BOARDS] = { BOARD_WIN_COM8, BOARD_UNKNOWN,
	BOARD_UNKNOWN };
MODULE_PARM(brdtype, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(brdtype, "type of each board, see dsm_serial.h");

/* default ioport addresses */
static int ioport[MAX_NUM_BOARDS] = { 0x300, 0x340, 0x380};
MODULE_PARM(ioport, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(ioport, "IOPORT address of each serial card");

/*
 * each UART on card has its own ioport with a width of 8 bytes.
 * We put them in a continuous block starting at ioport0[board],
 * of total length (8 * numports) where board is the board number,
 * and numports is the number of UARTS on a board.
 */
static int ioport0[MAX_NUM_BOARDS] = { 0x100, 0x140, 0x180};
MODULE_PARM(ioport0, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(ioport0, "IOPORT of first UART on each board");

/* IRQs.  Each board can use a different IRQ.
 * This could be enhanced to allow the ports on a
 * board to use different irqs, but it wouldn't help
 * throughput much, since these boards have an
 * interrupt id register (COM8_BC_IIR) that provides a bit
 * mask for the interrupting ports.
 */
static int irq_param[MAX_NUM_BOARDS] = { 11,0,0 };
MODULE_PARM(irq_param, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(irq_param, "IRQ number");

static struct serialBoard *boardInfo = 0;

unsigned int dsm_serial_irq_handler(unsigned int irq,
	void* callbackptr, struct rtl_frame *regs);

static unsigned int dsm_port_irq_handler(unsigned int irq,struct serialPort* port);

/* the resolution of GET_MSEC_CLOCK */
static int clock_res_msec;
static int clock_res_msec_o2;	// over 2

/*
 * Here we define the default xmit fifo size used for each type of
 * UART
 */
static struct serial_uart_config uart_config[] = {
        { "unknown", 1, 0 },
        { "8250", 1, 0 },
        { "16450", 1, 0 },
        { "16550", 1, 0 },
        { "16550A", 16, UART_CLEAR_FIFO | UART_USE_FIFO },
        { "cirrus", 1, 0 },     /* usurped by cyclades.c */
        { "ST16650", 1, UART_CLEAR_FIFO | UART_STARTECH },
        { "ST16650V2", 32, UART_CLEAR_FIFO | UART_USE_FIFO |
                  UART_STARTECH },
        { "TI16750", 64, UART_CLEAR_FIFO | UART_USE_FIFO},
        { "Startech", 1, 0},    /* usurped by cyclades.c */
        { "16C950/954", 128, UART_CLEAR_FIFO | UART_USE_FIFO},
        { "ST16654", 64, UART_CLEAR_FIFO | UART_USE_FIFO |
                  UART_STARTECH },
        { "XR16850", 128, UART_CLEAR_FIFO | UART_USE_FIFO |
                  UART_STARTECH },
        { "RSA", 2048, UART_CLEAR_FIFO | UART_USE_FIFO },
        { 0, 0}
};


#define serial_inp(port,offset) inb((port)->addr + (offset))
#define serial_outp(port,offset,value) outb((value),(port)->addr + (offset))

#define serial_in(port,offset) inb((port)->addr + (offset))
#define serial_out(port,offset,value) outb((value),(port)->addr + (offset))
/*
 * For the 16C950
 */
void dsm_serial_icr_write(struct serialPort *port, int offset, int  value)
{
        serial_out(port, UART_SCR, offset);
        serial_out(port, UART_ICR, value);
}
                                                                                
unsigned int dsm_serial_icr_read(struct serialPort *port, int offset)
{
        int     value;
                                                                                
        dsm_serial_icr_write(port, UART_ACR, port->ACR | UART_ACR_ICRRD);
        serial_out(port, UART_SCR, offset);
        value = serial_in(port, UART_ICR);
        dsm_serial_icr_write(port, UART_ACR, port->ACR);
        return value;
}


/*
 * Routine which returns the baud rate of the tty
 *
 * Note that the baud_table needs to be kept in sync with the
 * include/asm/termbits.h file.
 */
static int baud_table[] = {
        0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
        9600, 19200, 38400, 57600, 115200, 230400, 460800,
        500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000,
        2500000, 3000000, 3500000, 4000000
};
                                                                                
static int n_baud_table = sizeof(baud_table)/sizeof(int);
                                                                                
static int termios_get_baud_rate(struct termios* termios)
{
        unsigned int cflag, i;
                                                                                
        cflag = termios->c_cflag;
                                                                                
        i = cflag & CBAUD;
#ifdef DEBUG
	DSMLOG_DEBUG("get_baud_rate, i=%d, CBAUD=0x%x\n",i,CBAUD);
#endif
        if (i & CBAUDEX) {
#ifdef DEBUG
		DSMLOG_DEBUG("get_baud_rate, CBAUDEX\n");
#endif
                i &= ~CBAUDEX;
                if (i < 1 || i+15 >= n_baud_table)
                        termios->c_cflag &= ~CBAUDEX;
                else
                        i += 15;
        }
#ifdef DEBUG
	DSMLOG_DEBUG("get_baud_rate, i=%d\n",i);
#endif
                                                                                
        return baud_table[i];
}

/*
 * This is a quickie test to see how big the FIFO is.
 * It doesn't work at all the time, more's the pity.
 */
static int size_fifo(struct serialPort *port)
{
    unsigned char old_fcr, old_mcr, old_dll, old_dlm;
    int count;
									    
    old_fcr = serial_inp(port, UART_FCR);
    old_mcr = serial_inp(port, UART_MCR);
    serial_outp(port, UART_FCR, UART_FCR_ENABLE_FIFO |
		UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
    serial_outp(port, UART_MCR, UART_MCR_LOOP);
    serial_outp(port, UART_LCR, UART_LCR_DLAB);
    old_dll = serial_inp(port, UART_DLL);
    old_dlm = serial_inp(port, UART_DLM);
    serial_outp(port, UART_DLL, 0x01);
    serial_outp(port, UART_DLM, 0x00);
    serial_outp(port, UART_LCR, 0x03);
    for (count = 0; count < 256; count++)
	    serial_outp(port, UART_TX, count);
    mdelay(20);
    for (count = 0; (serial_inp(port, UART_LSR) & UART_LSR_DR) &&
	 (count < 256); count++)
	    serial_inp(port, UART_RX);
    serial_outp(port, UART_FCR, old_fcr);
    serial_outp(port, UART_MCR, old_mcr);
    serial_outp(port, UART_LCR, UART_LCR_DLAB);
    serial_outp(port, UART_DLL, old_dll);
    serial_outp(port, UART_DLM, old_dlm);
									    
    return count;
}


/*
 * This is a helper routine to autodetect StarTech/Exar/Oxsemi UART's.
 * When this function is called we know it is at least a StarTech
 * 16650 V2, but it might be one of several StarTech UARTs, or one of
 * its clones.  (We treat the broken original StarTech 16650 V1 as a
 * 16550, and why not?  Startech doesn't seem to even acknowledge its
 * existence.)
 *
 * What evil have men's minds wrought...
 */
static void autoconfig_startech_uarts(struct serialPort *port)
{
    unsigned char scratch, scratch2, scratch3, scratch4;
									    
    /*
     * First we check to see if it's an Oxford Semiconductor UART.
     *
     * If we have to do this here because some non-National
     * Semiconductor clone chips lock up if you try writing to the
     * LSR register (which serial_icr_read does)
     */
    if (port->type == PORT_16550A) {
	    /*
	     * EFR [4] must be set else this test fails
	     *
	     * This shouldn't be necessary, but Mike Hudson
	     * (Exoray@isys.ca) claims that it's needed for 952
	     * dual UART's (which are not recommended for new designs).
	     */
	    port->ACR = 0;
	    serial_out(port, UART_LCR, 0xBF);
	    serial_out(port, UART_EFR, 0x10);
	    serial_out(port, UART_LCR, 0x00);
	    /* Check for Oxford Semiconductor 16C950 */
	    scratch = dsm_serial_icr_read(port, UART_ID1);
	    scratch2 = dsm_serial_icr_read(port, UART_ID2);
	    scratch3 = dsm_serial_icr_read(port, UART_ID3);
									    
	    if (scratch == 0x16 && scratch2 == 0xC9 &&
		(scratch3 == 0x50 || scratch3 == 0x52 ||
		 scratch3 == 0x54)) {
		    port->type = PORT_16C950;
		    port->revision = dsm_serial_icr_read(port, UART_REV) |
			    (scratch3 << 8);
		    return;
	    }
    }
									    
    /*
     * We check for a XR16C850 by setting DLL and DLM to 0, and
     * then reading back DLL and DLM.  If DLM reads back 0x10,
     * then the UART is a XR16C850 and the DLL contains the chip
     * revision.  If DLM reads back 0x14, then the UART is a
     * XR16C854.
     *
     */
									    
    /* Save the DLL and DLM */
									    
    serial_outp(port, UART_LCR, UART_LCR_DLAB);
    scratch3 = serial_inp(port, UART_DLL);
    scratch4 = serial_inp(port, UART_DLM);
									    
    serial_outp(port, UART_DLL, 0);
    serial_outp(port, UART_DLM, 0);
    scratch2 = serial_inp(port, UART_DLL);
    scratch = serial_inp(port, UART_DLM);
    serial_outp(port, UART_LCR, 0);
									    
    if (scratch == 0x10 || scratch == 0x14) {
	    port->revision = scratch2;
	    port->type = PORT_16850;
	    return;
    }
									    
    /* Restore the DLL and DLM */
									    
    serial_outp(port, UART_LCR, UART_LCR_DLAB);
    serial_outp(port, UART_DLL, scratch3);
    serial_outp(port, UART_DLM, scratch4);
    serial_outp(port, UART_LCR, 0);
    /*
     * We distinguish between the '654 and the '650 by counting
     * how many bytes are in the FIFO.  I'm using this for now,
     * since that's the technique that was sent to me in the
     * serial driver update, but I'm not convinced this works.
     * I've had problems doing this in the past.  -TYT
     */
    if (size_fifo(port) == 64)
	    port->type = PORT_16654;
    else
	    port->type = PORT_16650V2;
}

/*
 * This routine is called by rs_init() to initialize a specific serial
 * port.  It determines what type of UART chip this serial port is
 * using: 8250, 16450, 16550, 16550A.  The important question is
 * whether or not this UART is a 16550A or not, since this will
 * determine whether or not we can use its FIFO features or not.
 * Return: negative RTL errno
 */
static int autoconfig(struct serialPort* port)
{
    unsigned char status1, status2, scratch, scratch2, scratch3;
    unsigned char save_lcr, save_mcr;
    unsigned long flags;
									    
    port->type = PORT_UNKNOWN;
									    
#ifdef SERIAL_DEBUG_AUTOCONF
    DSMLOG_DEBUG("Testing dsmser%d (0x%04lx)...\n", port->portNum,
	   port->addr - SYSTEM_ISA_IOPORT_BASE);
#endif
									    
    // if (!CONFIGURED_SERIAL_PORT(port)) return;
									    
    rtl_spin_lock_irqsave(&port->lock,flags);
									    
    /*
     * Do a simple existence test first; if we fail this,
     * there's no point trying anything else.
     *
     * 0x80 is used as a nonsense port to prevent against
     * false positives due to ISA bus float.  The
     * assumption is that 0x80 is a non-existent port;
     * which should be safe since include/asm/io.h also
     * makes this assumption.
     */
    scratch = serial_inp(port, UART_IER);
    serial_outp(port, UART_IER, 0);
#if defined(__i386__) || defined(__arm__)
    outb(0xff, 0x080 + SYSTEM_ISA_IOPORT_BASE);
#endif
    scratch2 = serial_inp(port, UART_IER);
    serial_outp(port, UART_IER, 0x0F);
#if defined(__i386__) || defined(__arm__)
    outb(0, 0x080 + SYSTEM_ISA_IOPORT_BASE);
#endif
    scratch3 = serial_inp(port, UART_IER);
    serial_outp(port, UART_IER, scratch);
    if (scratch2 || scratch3 != 0x0F) {
#ifdef SERIAL_DEBUG_AUTOCONF
	DSMLOG_WARNING("dsmser%d: simple autoconfig failed "
	       "(%02x, %02x)\n", port->portNum,
	       scratch2, scratch3);
#endif
	rtl_spin_unlock_irqrestore(&port->lock,flags);
	return -RTL_ENODEV;         /* We failed; there's nothing here */
    }

    // rtl_printf("detected serial port\n");
									    
    save_mcr = serial_in(port, UART_MCR);
    save_lcr = serial_in(port, UART_LCR);
									    
    /*
     * Check to see if a UART is really there.  Certain broken
     * internal modems based on the Rockwell chipset fail this
     * test, because they apparently don't implement the loopback
     * test mode.  So this test is skipped on the COM 1 through
     * COM 4 ports.  This *should* be safe, since no board
     * manufacturer would be stupid enough to design a board
     * that conflicts with COM 1-4 --- we hope!
     */
    serial_outp(port, UART_MCR, UART_MCR_LOOP | 0x0A);
    status1 = serial_inp(port, UART_MSR) & 0xF0;
    serial_outp(port, UART_MCR, save_mcr);
    if (status1 != 0x90) {
#ifdef SERIAL_DEBUG_AUTOCONF
	DSMLOG_WARNING("dsmser%d: no UART loopback failed\n",
	       port->portNum);
#endif
	rtl_spin_unlock_irqrestore(&port->lock,flags);
	return -RTL_ENODEV;
    }
    serial_outp(port, UART_LCR, 0xBF); /* set up for StarTech test */
    serial_outp(port, UART_EFR, 0); /* EFR is the same as FCR */
    serial_outp(port, UART_LCR, 0);
    serial_outp(port, UART_FCR, UART_FCR_ENABLE_FIFO);
    scratch = serial_in(port, UART_IIR) >> 6;
    switch (scratch) {
	    case 0:
		    port->type = PORT_16450;
		    break;
	    case 1:
		    port->type = PORT_UNKNOWN;
		    break;
	    case 2:
		    port->type = PORT_16550;
		    break;
	    case 3:
		    port->type = PORT_16550A;
		    break;
    }
    if (port->type == PORT_16550A) {
	    /* Check for Startech UART's */
	    serial_outp(port, UART_LCR, UART_LCR_DLAB);
	    if (serial_in(port, UART_EFR) == 0) {
		    serial_outp(port, UART_EFR, 0xA8);
		    if (serial_in(port, UART_EFR) == 0) {
			    /* We are a NS16552D/Motorola
			     * 8xxx DUART, stop. */
			    goto out;
		    }
		    port->type = PORT_16650;
		    serial_outp(port, UART_EFR, 0);
	    } else {
		    serial_outp(port, UART_LCR, 0xBF);
		    if (serial_in(port, UART_EFR) == 0)
			    autoconfig_startech_uarts(port);
	    }
    }
    if (port->type == PORT_16550A) {
	    /* Check for TI 16750 */
	    serial_outp(port, UART_LCR, save_lcr | UART_LCR_DLAB);
	    serial_outp(port, UART_FCR,
			UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
	    scratch = serial_in(port, UART_IIR) >> 5;
	    if (scratch == 7) {
		    /*
		     * If this is a 16750, and not a cheap UART
		     * clone, then it should only go into 64 byte
		     * mode if the UART_FCR7_64BYTE bit was set
		     * while UART_LCR_DLAB was latched.
		     */
		    serial_outp(port, UART_FCR, UART_FCR_ENABLE_FIFO);
		    serial_outp(port, UART_LCR, 0);
		    serial_outp(port, UART_FCR,
				UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
		    scratch = serial_in(port, UART_IIR) >> 5;
		    if (scratch == 6)
			    port->type = PORT_16750;
	    }
	    serial_outp(port, UART_FCR, UART_FCR_ENABLE_FIFO);
    }
out:
    serial_outp(port, UART_LCR, save_lcr);
    if (port->type == PORT_16450) {
	    scratch = serial_in(port, UART_SCR);
	    serial_outp(port, UART_SCR, 0xa5);
	    status1 = serial_in(port, UART_SCR);
	    serial_outp(port, UART_SCR, 0x5a);
	    status2 = serial_in(port, UART_SCR);
	    serial_outp(port, UART_SCR, scratch);
									    
	    if ((status1 != 0xa5) || (status2 != 0x5a))
		    port->type = PORT_8250;
    }
    port->xmit_fifo_size = uart_config[port->type].dfl_xmit_fifo_size;
									    
    if (port->type == PORT_UNKNOWN) {
	    rtl_spin_unlock_irqrestore(&port->lock,flags);
	    return -RTL_ENODEV;
    }
									    
    /*
    if (port->addr) {
		    request_region(port->addr,8,"serial(auto)");
    }
    */
									    
    /*
     * Reset the UART.
     */
    serial_outp(port, UART_MCR, save_mcr);
    serial_outp(port, UART_FCR, (UART_FCR_ENABLE_FIFO |
				 UART_FCR_CLEAR_RCVR |
				 UART_FCR_CLEAR_XMIT));
#ifdef DEBUG
    DSMLOG_DEBUG("cleared FIFO\n");
#endif
    serial_outp(port, UART_FCR, 0);
#ifdef DEBUG
    DSMLOG_DEBUG("disabled FIFO\n");
#endif
    (void)serial_in(port, UART_RX);
    serial_outp(port, UART_IER, 0);
									    
    rtl_spin_unlock_irqrestore(&port->lock,flags);

#ifdef DEBUG
    DSMLOG_DEBUG("uart is a %s\n",uart_config[port->type].name);
#endif
    return 0;
}

static void flush_port_nolock(struct serialPort* port)
{
    port->pe_cnt = 0;
    port->oe_cnt = 0;
    port->fe_cnt = 0;
    port->input_chars_lost = 0;
    port->output_chars_lost = 0;
    port->sample_overflows = 0;
    port->uart_sample_overflows = 0;
    port->sepcnt = 0;
    port->xmit.head = port->xmit.tail = 0;
    port->uart_samples.head = port->uart_samples.tail = 0;
    port->output_samples.head = port->output_samples.tail = 0;
    port->unwrittenp = 0;
    port->unwrittenl = 0;
    port->cosamp = 0;
    port->max_fifo_usage = 0;
    port->min_fifo_usage = UART_SAMPLE_SIZE;
    port->promptOn = 0;
}

/*
 * Exar XR16C864s (and possibly other UARTs) have the capability
 * of interrupting the CPU on receipt of a special character.
 * We'll use the last character of the record separator,
 * typically a NL or CR for ASCII data, as the interrupt
 * character. That should provide more timely notice of the
 * receipt of a record.
 */
static int set_interrupt_char(struct serialPort* port, unsigned char c)
{
    if (port->type == PORT_16850) {
	unsigned char efr,lcr;

	lcr = serial_in(port, UART_LCR);
	serial_outp(port, UART_LCR, 0xBF);

	// On 16C850's the EMSR register becomes XOFF2 when LCR=0xBF
	serial_outp(port,UART_EMSR,c);	// set xoff2 character
#ifdef DEBUG
	DSMLOG_DEBUG("c=0x%x, xoff2=0x%x\n",c,serial_in(port,UART_EMSR));
#endif

	efr = serial_in(port, UART_EFR);
#ifdef DEBUG
	DSMLOG_DEBUG("efr=0x%x\n",efr);
#endif
	// Enable special character detect, and enhanced function bits
	efr |= UART_EFR_SCD | UART_EFR_ECB;
#ifdef DEBUG
	DSMLOG_DEBUG("efr=0x%x\n",efr);
#endif
	serial_outp(port, UART_EFR,efr);

	serial_outp(port, UART_LCR, lcr);

	port->IER |= 0x20;	// XOFF interrupt enable (needs a #define)
	serial_out(port, UART_IER, port->IER);
    }
    else DSMLOG_WARNING("special character interrupts not supported on this UART\n");
    return 0;
}

static int unset_interrupt_char(struct serialPort* port)
{
    if (port->type == PORT_16850) {
	unsigned char efr,lcr;

	lcr = serial_in(port, UART_LCR);
	serial_outp(port, UART_LCR, 0xBF);

	// On 16C850's the EMSR register becomes XOFF2 when LCR=0xBF
	serial_outp(port,UART_EMSR,0);	// set xoff2 character
#ifdef DEBUG
	DSMLOG_DEBUG("c=0x%x, xoff2=0x%x\n",c,serial_in(port,UART_EMSR));
#endif

	efr = serial_in(port, UART_EFR);
#ifdef DEBUG
	DSMLOG_DEBUG("efr=0x%x\n",efr);
#endif
	// Disable special character detect, and enhanced function bits
	efr &= ~UART_EFR_SCD & ~UART_EFR_ECB;
#ifdef DEBUG
	DSMLOG_DEBUG("efr=0x%x\n",efr);
#endif
	serial_outp(port, UART_EFR,efr);

	serial_outp(port, UART_LCR, lcr);

	port->IER &= ~0x20;	// XOFF interrupt disable
	serial_out(port, UART_IER, port->IER);
    }
    return 0;
}
/*
 * Taken from linux drivers/char/serial.c change_speed
 * Does not do a spin_lock on port->lock.
 * Return: negative RTL errno
 */
static int change_speed(struct serialPort* port, struct termios* termios)
{
    int quot = 0, baud_base, baud;
    unsigned cflag,cval,fcr=0;
    int bits;

    cflag = termios->c_cflag;

    switch (cflag & CSIZE) {
	case CS5: cval = 0x00; bits = 7; break;
	case CS6: cval = 0x01; bits = 8; break;
	case CS7: cval = 0x02; bits = 9; break;
	case CS8: cval = 0x03; bits = 10; break;
        /* Never happens, but GCC is too dumb to figure it out */
	default:  cval = 0x00; bits = 7; break;
    }
    if (cflag & CSTOPB) {
	    cval |= 0x04;
	    bits++;
    }
    if (cflag & PARENB) {
	    cval |= UART_LCR_PARITY;
	    bits++;
    }
    if (!(cflag & PARODD))
	    cval |= UART_LCR_EPAR;

    /* Determine divisor based on baud rate */
    baud = termios_get_baud_rate(termios);
#ifdef DEBUG
    DSMLOG_DEBUG("baud=%d\n",baud);
#endif
    if (!baud)
	baud = 9600;    /* B0 transition handled in rs_set_termios */

    baud_base = port->baud_base;

    if (port->type == PORT_16C950) {
	    if (baud <= baud_base)
		    dsm_serial_icr_write(port, UART_TCR, 0);
	    else if (baud <= 2*baud_base) {
		    dsm_serial_icr_write(port, UART_TCR, 0x8);
		    baud_base = baud_base * 2;
	    } else if (baud <= 4*baud_base) {
		    dsm_serial_icr_write(port, UART_TCR, 0x4);
		    baud_base = baud_base * 4;
	    } else
		    dsm_serial_icr_write(port, UART_TCR, 0);
    }
    if (baud == 38400 &&
	((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST))
	    quot = port->custom_divisor;
    else {
	if (baud == 134)
		/* Special case since 134 is really 134.5 */
		quot = (2*baud_base / 269);
	else if (baud)
		quot = baud_base / baud;
    }
    /* As a last resort, if the quotient is zero, default to 9600 bps */
    if (!quot)
	    quot = baud_base / 9600;

    /*
     * Work around a bug in the Oxford Semiconductor 952 rev B
     * chip which causes it to seriously miscalculate baud rates
     * when DLL is 0.
     */
    if (((quot & 0xFF) == 0) && (port->type == PORT_16C950) &&
	(port->revision == 0x5201))
	    quot++;

    port->quot = quot;
    port->timeout = ((port->xmit_fifo_size*HZ*bits*quot) / baud_base);
    port->timeout += HZ/50;         /* Add .02 seconds of slop */

    port->usecs_per_char = (bits * USECS_PER_SEC + baud/2) / baud;

    /* Set up FIFO's */
    if (uart_config[port->type].flags & UART_USE_FIFO) {
	if ((port->baud_base / quot) < 2400)
	    fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1;
	else
	    fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_8;
    }
    if (port->type == PORT_16750)
	    fcr |= UART_FCR7_64BYTE;

    /* CTS flow control flag and modem status interrupts */
    port->IER &= ~UART_IER_MSI;
    if (port->flags & ASYNC_HARDPPS_CD)
	    port->IER |= UART_IER_MSI;

    if (cflag & CRTSCTS) {
	    port->flags |= ASYNC_CTS_FLOW;
	    port->IER |= UART_IER_MSI;
    } else
	    port->flags &= ~ASYNC_CTS_FLOW;
    if (cflag & CLOCAL)
	    port->flags &= ~ASYNC_CHECK_CD;
    else {
	    port->flags |= ASYNC_CHECK_CD;
	    port->IER |= UART_IER_MSI;
    }

    serial_out(port, UART_IER, port->IER);

    if (uart_config[port->type].flags & UART_STARTECH) {
	    serial_outp(port, UART_LCR, 0xBF);
	    serial_outp(port, UART_EFR,
			(cflag & CRTSCTS) ? UART_EFR_CTS : 0);
    }

#ifdef DEBUG
    DSMLOG_DEBUG("baud=%d, quot=%d, 0x%x\n",baud,quot,quot);
#endif


    serial_outp(port, UART_LCR, cval | UART_LCR_DLAB);      /* set DLAB */
    serial_outp(port, UART_DLL, quot & 0xff);       /* LS of divisor */
    serial_outp(port, UART_DLM, quot >> 8);         /* MS of divisor */
    serial_outp(port, UART_LCR, cval);              /* reset DLAB */
    port->LCR = cval;                               /* Save LCR */

    if (port->type != PORT_16750) {
	if (fcr & UART_FCR_ENABLE_FIFO) {
		/* emulated UARTs (Lucent Venus 167x) need two steps */
		serial_outp(port, UART_FCR, UART_FCR_ENABLE_FIFO);
	}
	serial_outp(port, UART_FCR, fcr);       /* set fcr */
#ifdef DEBUG
	DSMLOG_DEBUG("set FCR\n");
#endif
    }

    flush_port_nolock(port);

    return 0;
}
/*
 * Return: negative RTL errno
 */
static int uart_startup(struct serialPort* port)
{
    unsigned long flags;
    int retval = 0;

    if (port->type == PORT_UNKNOWN &&
    	(retval = autoconfig(port)) < 0) return retval;

    rtl_spin_lock_irqsave(&port->lock,flags);

    port->MCR = 0;
    if (port->termios.c_cflag & CBAUD)
	port->MCR = UART_MCR_DTR | UART_MCR_RTS;
    if (port->irq != 0) port->MCR |= UART_MCR_OUT2;

    if (uart_config[port->type].flags & UART_STARTECH) {
	    /* Wake up UART */
	    serial_outp(port, UART_LCR, 0xBF);
	    serial_outp(port, UART_EFR, UART_EFR_ECB);
	    /*
	     * Turn off LCR == 0xBF so we actually set the IER
	     * register on the XR16C850
	     */
	    serial_outp(port, UART_LCR, 0);
	    serial_outp(port, UART_IER, 0);
	    /*
	     * Now reset LCR so we can turn off the ECB bit
	     */
	    serial_outp(port, UART_LCR, 0xBF);
	    serial_outp(port, UART_EFR, 0);
	    /*
	     * For a XR16C850, we need to set the trigger levels
	     */
	    if (port->type == PORT_16850) {
		    // rtl_printf("setting trigger levels\n");
		    serial_outp(port, UART_FCTR, UART_FCTR_TRGD |
				    UART_FCTR_RX);
		    serial_outp(port, UART_TRG, UART_TRG_96);
		    serial_outp(port, UART_FCTR, UART_FCTR_TRGD |
				    UART_FCTR_TX);
		    serial_outp(port, UART_TRG, UART_TRG_96);
	    }
	    serial_outp(port, UART_LCR, 0);
    }
									    
    if (port->type == PORT_16750) {
	    /* Wake up UART */
	    serial_outp(port, UART_IER, 0);
    }
   if (port->type == PORT_16C950) {
	    /* Wake up and initialize UART */
	    port->ACR = 0;
	    serial_outp(port, UART_LCR, 0xBF);
	    serial_outp(port, UART_EFR, UART_EFR_ECB);
	    serial_outp(port, UART_IER, 0);
	    serial_outp(port, UART_LCR, 0);
	    dsm_serial_icr_write(port, UART_CSR, 0); /* Reset the UART */
	    serial_outp(port, UART_LCR, 0xBF);
	    serial_outp(port, UART_EFR, UART_EFR_ECB);
	    serial_outp(port, UART_LCR, 0);
    }
    /*
     * Clear the FIFO buffers and disable them
     * (they will be reenabled in change_speed())
     */
    if (uart_config[port->type].flags & UART_CLEAR_FIFO) {
	    serial_outp(port, UART_FCR, UART_FCR_ENABLE_FIFO);
	    serial_outp(port, UART_FCR, (UART_FCR_ENABLE_FIFO |
					 UART_FCR_CLEAR_RCVR |
					 UART_FCR_CLEAR_XMIT));
	    serial_outp(port, UART_FCR, 0);
#ifdef DEBUG
	    DSMLOG_DEBUG("uart_startup: cleared FIFO\n");
#endif
    }

    /*
     * Clear the interrupt registers.
     */
    (void) serial_inp(port, UART_LSR);
    (void) serial_inp(port, UART_RX);
    (void) serial_inp(port, UART_IIR);
    (void) serial_inp(port, UART_MSR);

    /*
     * At this point there's no way the LSR could still be 0xFF;
     * if it is, then bail out, because there's likely no UART
     * here.
     */
    if (!(port->flags & ASYNC_BUGGY_UART) &&
	(serial_inp(port, UART_LSR) == 0xff)) {
	    DSMLOG_WARNING("ttyS%d: LSR safety check engaged!\n", port->portNum);
	    retval = -RTL_ENODEV;
	    goto errout;
    }
                                                                                
    /*
     * Now, initialize the UART
     */
    serial_outp(port, UART_LCR, UART_LCR_WLEN8);    /* reset DLAB */

    // port->MCR |= ALPHA_KLUDGE_MCR;          /* Don't ask */
    serial_outp(port, UART_MCR, port->MCR);

    retval = change_speed(port,&port->termios);

    /*
     * Finally, enable interrupts
     */
    port->IER = UART_IER_MSI | UART_IER_RLSI | UART_IER_RDI;
    serial_outp(port, UART_IER, port->IER); /* enable interrupts */

    /*
     * And clear the interrupt registers again for luck.
     */
    (void)serial_inp(port, UART_LSR);
    (void)serial_inp(port, UART_RX);
    (void)serial_inp(port, UART_IIR);
    (void)serial_inp(port, UART_MSR);

errout:
    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return retval;
}

/*
 * Return: negative RTL errno
 */
static int tcgetattr(struct serialPort* port, void* buf)
{
    unsigned long flags;
    rtl_spin_lock_irqsave (&port->lock,flags);
    memcpy(buf,&port->termios,sizeof(struct termios));
    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return 0;
}

/*
 * Return: negative RTL errno
 */
static int tcsetattr(struct serialPort* port, struct termios* termios)
{
    unsigned long flags;

    rtl_spin_lock_irqsave (&port->lock,flags);

    int retval = change_speed(port,termios);

    memcpy(&port->termios,termios,sizeof(struct termios));

    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return retval;
}

static int uart_shutdown(struct serialPort* port)
{
    unsigned long flags;
    rtl_spin_lock_irqsave(&port->lock,flags);

    port->IER = 0;
    serial_outp(port, UART_IER, 0x00);      /* disable all intrs */

    port->MCR &= ~UART_MCR_OUT2;
    /* disable break condition */
    serial_out(port, UART_LCR, serial_inp(port, UART_LCR) & ~UART_LCR_SBC);

    if (port->termios.c_cflag & HUPCL)
	port->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
    serial_outp(port, UART_MCR, port->MCR);

    /* disable FIFO's */
    serial_outp(port, UART_FCR, (UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
	UART_FCR_CLEAR_XMIT));
    serial_outp(port, UART_FCR, 0);

    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return 0;
}

/*
 * copy characters into the transmit circular buffer for transmission
 * out of the uart.
 */
static rtl_ssize_t queue_transmit_chars(struct serialPort* port,
	const char* buf, rtl_size_t count)
{
    unsigned long flags;
    int left = count;
    int n;

    if (!count) return 0;

    rtl_spin_lock_irqsave(&port->lock,flags);

    n = CIRC_SPACE_TO_END(port->xmit.head,port->xmit.tail,SERIAL_XMIT_SIZE);
    if (!n) goto done;
    if (n > left) n = left;

    memcpy(port->xmit.buf + port->xmit.head,buf,n);
    port->xmit.head = (port->xmit.head + n) & (SERIAL_XMIT_SIZE-1);

    left -= n;

    if (!left) goto done;

    buf += n;
    n = CIRC_SPACE_TO_END(port->xmit.head,port->xmit.tail,SERIAL_XMIT_SIZE);
    if (!n) goto done;
    if (n > left) n = left;

    memcpy(port->xmit.buf + port->xmit.head,buf,n);
    port->xmit.head = (port->xmit.head + n) & (SERIAL_XMIT_SIZE-1);

    left -= n;

done:

    rtl_spin_unlock_irqrestore(&port->lock,flags);

    // enable transmit interrupts
    if (!(port->IER & UART_IER_THRI)) {
	 port->IER |= UART_IER_THRI;
	 serial_out(port, UART_IER, port->IER);
    }

    return count - left;		// characters copied
}

/*
 * Return: negative RTL errno
 */
static int set_prompt(struct serialPort* port,
	struct dsm_serial_prompt* prompt)
{
    unsigned long flags;
    rtl_spin_lock_irqsave (&port->lock,flags);
    memcpy(&port->prompt,prompt,sizeof(struct dsm_serial_prompt));
    rtl_spin_unlock_irqrestore(&port->lock,flags);
#ifdef DEBUG
    DSMLOG_DEBUG("prompt len=%d, rate=%d\n",
    	port->prompt.len,(int)port->prompt.rate);
#endif
    return 0;
}

static int get_prompt(struct serialPort* port,
	struct dsm_serial_prompt* prompt)
{
    unsigned long flags;
    rtl_spin_lock_irqsave (&port->lock,flags);
    memcpy(prompt,&port->prompt,sizeof(struct dsm_serial_prompt));
    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return 0;
}

/*
 * function that is registered as an IRIG callback to
 * be executed at a given rate.
 * Prompts the serial port with a string.
 */
static void port_prompter(void* privateData)
{
    struct serialPort* port = (struct serialPort*) privateData;
    int res;
    if ((res =
    	queue_transmit_chars(port,port->prompt.str,port->prompt.len)) !=
    		port->prompt.len) {
	port->output_chars_lost += (port->prompt.len - res);
    }
#ifdef DEBUG
    DSMLOG_DEBUG("queue_transmit_chars, len=%d, res=%d\n",
		    port->prompt.len,res);
#endif
}

/*
 * Start the port_prompter
 * Return: negative RTL errno
 */
static int start_prompter(struct serialPort* port)
{
    if (!port->promptOn)
	register_irig_callback(port_prompter,port->prompt.rate,port);
    port->promptOn = 1;
    return 0;
}

/*
 * Return: negative RTL errno
 */
static int stop_prompter(struct serialPort* port)
{
    unregister_irig_callback(port_prompter,port->prompt.rate,port);
    port->promptOn = 0;
    return 0;
}

static inline dsm_sample_time_t compute_time_tag(struct dsm_sample* usamp,int nchars,
	int usecs_per_char)
{
    int msecadj = (nchars * usecs_per_char + USECS_PER_MSEC/2) /
				  USECS_PER_MSEC;
    msecadj += clock_res_msec_o2;
    msecadj -= (msecadj % clock_res_msec);
    dsm_sample_time_t tt = (usamp->timetag + msecadj) % MSECS_PER_DAY;
#ifdef DEBUG
    DSMLOG_DEBUG("tt=%d,msecadj=%d,nchars=%d\n",tt,msecadj,nchars);
#endif
    return tt;
}
/*
 * Scan a sample from a UART FIFO, copying the characters
 * into the sample at the head of the output_sample queue.
 * Break samples by a beginning of message separator.
 * Timetag the output samples with the computed receipt
 * time of the beginning-of-messages character.
 */
static void scan_sep_bom(struct serialPort* port,struct dsm_sample* usamp)
{
    // initial conditions: sepcnt = 0;

    // initial samples in the output queue will have length==0

    struct dsm_sample* osamp =
    	GET_HEAD(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
    if (!osamp) {		// no output sample available
        port->input_chars_lost += usamp->length;
	return;
    }

    register char* cp = usamp->data;
    char* ep = cp + usamp->length;

    for ( ; cp < ep; ) {
	// loop until we've figured out what to do with this character
        register char c = *cp;
	for (;;) {
	    if (port->sepcnt < port->recinfo.sepLen) {
		// This block is entered if we are currently scanning
		// for the message separator at the beginning of the message.
		if (c == port->recinfo.sep[port->sepcnt]) {
		    // the receipt time of the initial character is the
		    // timetag for the sample.
		    if (port->sepcnt == 0) port->bomtt =
		    	compute_time_tag(usamp,cp-usamp->data,
				port->usecs_per_char);
                    cp++;
		    // We now have a character match to the record separator.
		    // increment the separator counter.
		    // if matched entire separator string, previous sample
		    // is ready, and increment the head of the queue.

		    if (++port->sepcnt == port->recinfo.sepLen) {
                        if (port->addNull) {
                            if (osamp->length >= MAX_DSM_SERIAL_SAMPLE_SIZE) {
                                port->sample_overflows++;
                                port->input_chars_lost++;
                                osamp->data[osamp->length-1] = '\0';
                            }
                            else osamp->data[osamp->length++] = '\0';
                        }
			if (osamp->length > 0)
			  osamp = NEXT_HEAD(port->output_samples,
				OUTPUT_SAMPLE_QUEUE_SIZE);
			if (!osamp) {	// no samples
			    port->input_chars_lost += (ep - cp);
			    return;
			}
			osamp->timetag = port->bomtt;
			// copy separator to next sample
			memcpy(osamp->data,port->recinfo.sep, port->sepcnt);
			osamp->length += port->sepcnt;
			// leave sepcnt equal to recinfo.sepLen
		    }
		    break;		// character was input
		}
		else {
		    // At this point:
		    // 1. we're expecting the BOM separator, but
		    // 2. the current character fails a match with the
		    //    BOM string
		    // We'll send the faulty data along anyway so that
		    // the user can see what is going on.

		    if (port->sepcnt > 0) {	// previous partial match
			// avoid overflow
			if (osamp->length + port->sepcnt >=
				MAX_DSM_SERIAL_SAMPLE_SIZE) {
                            if (port->addNull)
                                osamp->data[osamp->length-1] = '\0';
			    port->sample_overflows++;
			    osamp = NEXT_HEAD(port->output_samples,
				    OUTPUT_SAMPLE_QUEUE_SIZE);
			    if (!osamp) {
				port->input_chars_lost += (ep - cp);
				return;
			    }
			    osamp->timetag =
			    	compute_time_tag(usamp,cp-usamp->data,
					port->usecs_per_char);
			}
			// We have a partial match, copy them to the sample data.
			memcpy(osamp->data + osamp->length,
				port->recinfo.sep, port->sepcnt);
			osamp->length += port->sepcnt;
			port->sepcnt = 0;
			// keep trying matching first character of separator
			// this isn't infinitely recursive because now sepcnt=0
		    }
		    else {		// no match to first character in sep
			// avoid array overflow
			if (osamp->length >= MAX_DSM_SERIAL_SAMPLE_SIZE) {
			    port->sample_overflows++;
                            if (port->addNull)
                                osamp->data[osamp->length-1] = '\0';
			    osamp = NEXT_HEAD(port->output_samples,
				    OUTPUT_SAMPLE_QUEUE_SIZE);
			    if (!osamp) {
				port->input_chars_lost += (ep - cp);
				return;
			    }
			}

			if (osamp->length == 0)
			    osamp->timetag =
			    	compute_time_tag(usamp,cp-usamp->data,
					port->usecs_per_char);
			osamp->data[osamp->length++] = c;
                        cp++;
			break;			// character was input
		    }
		}
	    }	// searching for BOM string
	    else {
		// At this point we have a match to the BOM separater string.
		// and are filling the data buffer.

                // fixed record length, save characters in buffer.
                // no chance for overflow here, as long as
                // recordLen < sizeof(buffer)
                if (osamp->length <
                    port->recinfo.recordLen + port->recinfo.sepLen) {
                    int nc = MIN(port->recinfo.recordLen +
                        port->recinfo.sepLen - osamp->length,ep-cp);
                    memcpy(osamp->data + osamp->length,cp,nc);
                    cp += nc;
                    osamp->length += nc;
                    break;
                }

		// check to see if the character matches the
                // initial character in the separator string.
                if (c == port->recinfo.sep[0]) {	// first char of next
                    port->sepcnt = 0;
                    // loop again to try match to first character
                }
                else {
                    // no match, treat as data
                    // make sure there is room
                    if (osamp->length == MAX_DSM_SERIAL_SAMPLE_SIZE) {
                        if (port->addNull)
                            osamp->data[osamp->length-1] = '\0';
                        port->sample_overflows++;
                        osamp = NEXT_HEAD(port->output_samples,
                                OUTPUT_SAMPLE_QUEUE_SIZE);
                        if (!osamp) {
                            port->input_chars_lost += (ep - cp);
                            return;
                        }
                        port->sepcnt = 0;
                        osamp->timetag =
                            compute_time_tag(usamp,cp-usamp->data,
                                    port->usecs_per_char);
                    }
                    osamp->data[osamp->length++] = c;
                    cp++;
                    break;
                }
	    }
	}	// loop until we do something with character
    }		// loop over characters in uart sample
}

static void scan_sep_eom(struct serialPort* port,struct dsm_sample* usamp)
{
    // initial conditions: port->sepcnt = 0;

    struct dsm_sample* osamp =
    	GET_HEAD(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
    if (!osamp) {		// no output sample available
        port->input_chars_lost += usamp->length;
	return;
    }
    // samples fetched from the queue will have length==0

    register char* cp = usamp->data;
    char* ep = usamp->data + usamp->length;

    for ( ; cp < ep; ) {

	if (osamp->length == 0)
	    osamp->timetag =
		compute_time_tag(usamp,cp-usamp->data,port->usecs_per_char);

        // fixed length record, memcpy what we can
        if (osamp->length < port->recinfo.recordLen) {
            int nc = MIN(port->recinfo.recordLen - osamp->length,ep-cp);
            memcpy(osamp->data + osamp->length,cp,nc);
            cp += nc;
            osamp->length += nc;
        }
        else {
            // at this point osamp->length >= port->recinfo.recordLen
            // we've read in all our characters, now scan separator string
            // if no match, check from beginning of separator string

            // check for overflow
            if (osamp->length + port->addNull == MAX_DSM_SERIAL_SAMPLE_SIZE) {
                port->sample_overflows++;
                // send this bogus sample on
                if (port->addNull)
                    osamp->data[osamp->length++] = '\0';
                osamp = NEXT_HEAD(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
                if (!osamp) {
                    port->input_chars_lost += (ep - cp);
                    return;
                }
                port->sepcnt = 0;
                continue;
            }

            char c = *cp++;
            osamp->data[osamp->length++] = c;

	    // check to see if the character matches the current character
	    // in the end of message separator string.
	    if (c == port->recinfo.sep[port->sepcnt]) {
		// character matched
		if (++port->sepcnt == port->recinfo.sepLen) {   // done
		    if (port->addNull)
			osamp->data[osamp->length++] = '\0';
		    osamp = NEXT_HEAD(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
		    if (!osamp) {
			port->input_chars_lost += (ep - cp);
			return;
		    }
		    port->sepcnt = 0;
		}
	    }
	    else {
		// no match of current character to EOM string.

		// check for match at beginning of separator string
		// since sepcnt > 0 we won't have a complete match since
		// sepLen then must be > 1.
		if (port->sepcnt > 0 &&
		      c == port->recinfo.sep[port->sepcnt = 0]) port->sepcnt++;
	    }
        }
    }
}

/*
 * Scan a sample from a UART FIFO, copying the characters
 * into the sample at the head of the output_sample queue.
 * Break samples by a beginning of message separator.
 * Timetag the output samples with the computed receipt
 * time of the beginning-of-messages character.
 */
static void scan_by_length(struct serialPort* port,struct dsm_sample* usamp)
{
    // initial samples in the output queue will have length==0
    struct dsm_sample* osamp =
    	GET_HEAD(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
    if (!osamp) {		// no output sample available
        port->input_chars_lost += usamp->length;
	return;
    }

    register char* op = osamp->data + osamp->length;
    register char* cp = usamp->data;
    int left;

#ifdef DEBUG
    DSMLOG_DEBUG("in length=%d data=%#02x %#02x %#02x %#02x\n",
        usamp->length,(unsigned)usamp->data[0],(unsigned)usamp->data[1],
            (unsigned)usamp->data[2],(unsigned)usamp->data[3]);
    DSMLOG_DEBUG("out length=%d\n", osamp->length);
#endif

    if (osamp->length == 0)
        osamp->timetag =
            compute_time_tag(usamp,cp-usamp->data,port->usecs_per_char);

    for (left = usamp->length; left > 0;) {
        int nc = port->recinfo.recordLen - osamp->length;
        if (nc > left) nc = left;

        memcpy(op,cp,nc);
        op += nc;
        cp += nc;
        left -= nc;
        osamp->length += nc;
        if (osamp->length == port->recinfo.recordLen) {

#ifdef DEBUG
            DSMLOG_DEBUG("read sample, out length=%d data=%#02x %#02x %#02x %#02x\n",
                osamp->length,(unsigned)osamp->data[0],(unsigned)osamp->data[1],
                (unsigned)osamp->data[2],(unsigned)osamp->data[3]);
#endif
            osamp = NEXT_HEAD(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
            if (!osamp) {
                port->input_chars_lost += left;
                return;
            }
            osamp->timetag =
                compute_time_tag(usamp,cp-usamp->data,port->usecs_per_char);
            op = osamp->data;
        }
    }
}
/*
 * Return: negative RTL errno
 */
static int set_record_sep(struct serialPort* port,
	struct dsm_serial_record_info* sep)
{
    unsigned long flags;

#ifdef DEBUG
    DSMLOG_DEBUG("%s: set_record_sep, reclen=%d\n",port->devname,sep->recordLen);
#endif
    if (sep->recordLen + sep->sepLen + 1 >= MAX_DSM_SERIAL_SAMPLE_SIZE) {
	DSMLOG_ERR("%s: record size=%d + separator size=%d + 1 exceeds maximum = %d\n",
		port->devname,sep->recordLen,sep->sepLen,MAX_DSM_SERIAL_SAMPLE_SIZE);
        return -RTL_EINVAL;
    }
      
    rtl_spin_lock_irqsave (&port->lock,flags);
    memcpy(&port->recinfo,sep,sizeof(struct dsm_serial_record_info));

    port->sepcnt = 0;
    port->addNull = 0;

    // set the interrupt character to the last
    // character in the record separator
    if (sep->sepLen > 0) {

    	set_interrupt_char(port, sep->sep[sep->sepLen-1]);

	// If separator is a terminating NL or CR, then assume
	// these are ASCII records, and overwrite the termination
	// character with a NULL in order to simplify later
	// sscanf-ing.
        switch (sep->sep[sep->sepLen-1]) {
        case '\n':
        case '\r':
            port->addNull = 1;
            break;
        default:
            break;
        }
	if (sep->atEOM) port->scan_func = scan_sep_eom;
        else port->scan_func = scan_sep_bom;
    }
    else {
        port->scan_func = scan_by_length;
        unset_interrupt_char(port);
    }

    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return 0;
}

/*
 * Set the maximum time to wait when reading characters
 * from this serial device.  If characters are available
 * the the read will not block for more than val number
 * of microseconds.  This sets the timeout value 
 * for sample_sem.
 * Return: negative RTL errno
 */
static int set_latency_usec(struct serialPort* port, long val)
{
    unsigned long flags;
    rtl_spin_lock_irqsave (&port->lock,flags);

#ifdef DEBUG
    DSMLOG_DEBUG("latency=%d usecs\n",val);
#endif

    // screen latencies which are too small.
    // A latency of zero will cause the serial driver to hang!
    // We'll enforce a minimum of 1 millisec.
    if (val <= USECS_PER_MSEC) {
	long minval = USECS_PER_MSEC;
	DSMLOG_ERR("%s: illegal latency value = %d usecs, setting to %d usecs\n",
		port->devname,val, minval);
	val = minval;
    }
    port->read_timeout_sec = val / USECS_PER_SEC;
    port->read_timeout_nsec = (val % USECS_PER_SEC) * NSECS_PER_USEC;

    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return 0;
}

/*
 * Return: negative RTL errno
 */
static int get_record_sep(struct serialPort* port,
	struct dsm_serial_record_info* sep)
{
    unsigned long flags;
    rtl_spin_lock_irqsave (&port->lock,flags);
    memcpy(sep,&port->recinfo,sizeof(struct dsm_serial_record_info));
    rtl_spin_unlock_irqrestore(&port->lock,flags);
    return 0;
}

/*
 * Return: negative RTL errno
 */
static int get_status(struct serialPort* port,
	struct dsm_serial_status* status)
{
    /* Note we're not doing rtl_spin_lock_irqsave here */

#ifdef DEBUG
    DSMLOG_DEBUG("pe_cnt=%d,oe_cnt=%d\n",port->pe_cnt,port->oe_cnt);
#endif

    status->pe_cnt = port->pe_cnt;
    status->oe_cnt = port->oe_cnt;
    status->fe_cnt = port->fe_cnt;
    status->input_chars_lost = port->input_chars_lost;
    status->output_chars_lost = port->output_chars_lost;
    status->sample_overflows = port->sample_overflows;

    status->uart_queue_avail =
    	CIRC_SPACE(port->uart_samples.head,port->uart_samples.tail,
		UART_SAMPLE_QUEUE_SIZE);

    status->output_queue_avail =
    	CIRC_SPACE(port->output_samples.head,port->output_samples.tail,
		OUTPUT_SAMPLE_QUEUE_SIZE);

    status->char_xmit_queue_avail =
    	CIRC_SPACE(port->xmit.head, port->xmit.tail,SERIAL_XMIT_SIZE);

    status->max_fifo_usage = port->max_fifo_usage;
    port->max_fifo_usage = 0;

    if (port->min_fifo_usage == UART_SAMPLE_SIZE)
	status->min_fifo_usage = 0;
    else
	status->min_fifo_usage = port->min_fifo_usage;
    port->min_fifo_usage = UART_SAMPLE_SIZE;

    return 0;
}

/*
 * Return: negative RTL errno
 */
static int open_port(struct serialPort* port)
{
    int retval = 0;
    unsigned long flags;

    if (port->type == PORT_UNKNOWN &&
    	(retval = autoconfig(port)) < 0) return retval;
#ifdef DEBUG
    DSMLOG_DEBUG("open_port, uart=%s\n",uart_config[port->type]);
#endif

    retval = uart_startup(port);

    rtl_spin_lock_irqsave(&port->board->lock,flags);
    port->board->int_mask |= (1 << port->portIndex);
    rtl_spin_unlock_irqrestore(&port->board->lock,flags);

    return retval;
}

/*
 * Return: negative RTL errno
 */
static int close_port(struct serialPort* port)
{
    unsigned long flags;

    if (port->promptOn) stop_prompter(port);
    uart_shutdown(port);

    rtl_spin_lock_irqsave(&port->board->lock,flags);
    port->board->int_mask &= ~(1 << port->portIndex);
    rtl_spin_lock_irqsave(&port->board->lock,flags);

    return 0;
}

/*
 * Return: negative RTL errno
 */
static int write_eeprom(struct serialBoard* board)
{
    unsigned long flags;
    int ntry;
    int ip;

    rtl_spin_lock_irqsave(&board->lock,flags);

    /* first enable writes to EEPROM */
    serial_out(board,COM8_BC_ECR,0x30);
    serial_out(board,COM8_BC_CR,1);

    for (ntry = 10; ntry; ntry--) {
        unsigned long jwait = jiffies + 1;
	while (jiffies < jwait) schedule();

	if ((serial_in(board,COM8_BC_SR) & 0xc0) == 0x80) break;
    }
    if (!ntry) {
      DSMLOG_ERR("enable EEPROM write failed: timeout\n");
	rtl_spin_unlock_irqrestore(&board->lock,flags);
      return -RTL_EIO;
    }

    for (ip = 0; ip < board->numports; ip++) {
        struct serialPort* port = board->ports + ip;

	serial_out(board,COM8_BC_ECR,ip | 0x40);
	serial_out(board,COM8_BC_EHDR,(port->ioport >> 3) | 0x80);
	serial_out(board,COM8_BC_ELDR,(port->irq & 0xf));

	serial_out(board,COM8_BC_CR,1);

	for (ntry = 10; ntry; ntry--) {
	    unsigned long jwait = jiffies + 1;
	    while (jiffies < jwait) schedule();
	    if ((serial_in(board,COM8_BC_SR) & 0xc0) == 0x80) break;
	}
	if (!ntry) {
	  DSMLOG_ERR("writing config for port %d to EEPROM failed: timeout\n",
	  	ip);
	rtl_spin_unlock_irqrestore(&board->lock,flags);
	  return -RTL_EIO;
	}
    }

    /* disable writes to EEPROM */
    serial_out(board,COM8_BC_ECR,0x0);
    serial_out(board,COM8_BC_CR,1);

    for (ntry = 10; ntry; ntry--) {
	unsigned long jwait = jiffies + 1;
	while (jiffies < jwait) schedule();
	if ((serial_in(board,COM8_BC_SR) & 0xc0) == 0x80) break;
    }
    if (!ntry) {
      DSMLOG_ERR("enable EEPROM write failed: timeout\n");
	rtl_spin_unlock_irqrestore(&board->lock,flags);
      return -RTL_EIO;
    }
    rtl_spin_unlock_irqrestore(&board->lock,flags);
    return 0;
}

/*
 * initialize a struct serialPort (mostly just zero it out)
 */
static void init_serialPort_struct(struct serialPort* port)
{
    port->type = PORT_UNKNOWN;
    rtl_spin_lock_init (&port->lock);
    port->baud_base = 115200;
    port->xmit_fifo_size = 1;

    port->recinfo.atEOM = 1;
    port->scan_func = scan_sep_eom;

    /* semaphore timeout in read method.
     * Set from DSMSER_SET_LATENCY ioctl.
     */
    port->read_timeout_sec = 0;
    port->read_timeout_nsec = NSECS_PER_SEC / 10;	// default 1/10th sec

    rtl_sem_init(&port->sample_sem,0,0);

    rtl_memset(&port->termios, 0, sizeof(struct termios));
    port->termios.c_cflag = B38400 | CS8 | HUPCL;
}

/*
 * Send characters to the transmitter.  Called from interrupt code.
 */
static void transmit_chars(struct serialPort* port)
{
    int count;
    if (port->xmit.head == port->xmit.tail) {
	    port->IER &= ~UART_IER_THRI;
	    serial_out(port, UART_IER, port->IER);
	    return;
    }
														  
    count = port->xmit_fifo_size;
    do {
	    serial_out(port, UART_TX, port->xmit.buf[port->xmit.tail]);
	    port->xmit.tail = (port->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
	    if (port->xmit.head == port->xmit.tail)
		    break;
    } while (--count > 0);
														  
    // disable transmit interrupts if no data left
    if (port->xmit.head == port->xmit.tail) {
	    port->IER &= ~UART_IER_THRI;
	    serial_out(port, UART_IER, port->IER);
    }
}

/*
 * return: 1 = LSR OK
 *	   0 = LSR shows error condition
 */
int inline check_lsr(struct serialPort*port, unsigned char lsr)
{
    if (lsr & (UART_LSR_PE | UART_LSR_OE | UART_LSR_FE)) {
	if (lsr & UART_LSR_PE) port->pe_cnt++;	// parity errors
	if (lsr & UART_LSR_OE) port->oe_cnt++;	// input overflows
	if (lsr & UART_LSR_FE) port->fe_cnt++;	// framing errors
	// there is also a FIFO bit error here, not sure what
	// to do with it at this point.
	return 0;
    }
    return 1;
}

/*
 * Copy all available characters from the UART receive fifo
 * into a sample. This function is called from the uart
 * interrupt handler.
 * After copying the characters into the sample, the head
 * pointer of the sample queue is incremented and a semaphore
 * is posted so that the reader of this device can then
 * proceed to read & parse the sample at non-interrupt time.
 *
 * nchar_delay: estimate of the delay between when the
 * last character was received and when the interrupt service
 * routine was called.  A 16850 UART (and possibly others)
 * will generate an interrupt after 4 characters of dead time,
 * so nchar_delay in that case would be 4.
 */
static inline void receive_chars(struct serialPort* port,unsigned char* lsrp,
	int nchar_delay)
{
    /* check if a raw uart sample is available in the queue
     * for writing.  If the reader of this device has fallen behind
     * then this will be false, in which case we increment the
     * raw_samples_missed counter, and throw the characters away.
     */
    struct dsm_sample* samp =
    	GET_HEAD(port->uart_samples,UART_SAMPLE_QUEUE_SIZE);
    if (!samp) {		// no output sample available
	while(*lsrp & UART_LSR_DR) {
	    serial_in(port,UART_RX); // Read character
	    check_lsr(port,*lsrp);
	    *lsrp = serial_in(port,UART_LSR);
	    port->input_chars_lost++;
	}
	return;
    }

    samp->timetag = GET_MSEC_CLOCK;
    // Read uart fifo until empty
    register char* cp = samp->data;
    char* ep = cp + UART_SAMPLE_SIZE;
    while(*lsrp & UART_LSR_DR) {
	*cp = serial_in(port,UART_RX);	// Read character, good or bad
	if (cp < ep) cp++;
	check_lsr(port,*lsrp);
	*lsrp = serial_in(port,UART_LSR);
    }
    int nchars = cp - samp->data;
 
    // uart fifo bigger than expected
    if (cp == ep && port->uart_sample_overflows++ == 0)
	DSMLOG_ERR(
		"UART fifo exceeds %d characters. Increase UART_SAMPLE_SIZE\n",
		UART_SAMPLE_SIZE);

    if (port->max_fifo_usage < nchars) port->max_fifo_usage = nchars;
    if (port->min_fifo_usage > nchars) port->min_fifo_usage = nchars;

    samp->length = nchars;

    // adjust timetag earlier by the number of characters,
    // but only in multiples of the clock resolution,
    // otherwise we're fooling ourselves.
    int msecadj =
      ((nchars + nchar_delay) * port->usecs_per_char  + USECS_PER_MSEC/2) /
		  USECS_PER_MSEC;
    msecadj += clock_res_msec_o2;
    msecadj -= (msecadj % clock_res_msec);
    if (samp->timetag < msecadj) samp->timetag += MSECS_PER_DAY;
    samp->timetag -= msecadj;

#ifdef DEBUG_ISR
    DSMLOG_DEBUG("dev=%s,tt=%d,nchars=%d, nchar_delay=%d, msecadj=%d,usecs_per_char=%d\n",
    	port->devname,samp->timetag,nchars,nchar_delay,msecadj,port->usecs_per_char);

    struct rtl_timespec irigts;
    irig_clock_gettime(&irigts);
    DSMLOG_DEBUG("irigtt=%d\n",(irigts.tv_sec % SECS_PER_DAY) * MSECS_PER_SEC +
    	(irigts.tv_nsec / NSECS_PER_MSEC));
#endif

    /* increment head, this sample is ready for consumption */
    INCREMENT_HEAD(port->uart_samples,UART_SAMPLE_QUEUE_SIZE);
    rtl_sem_post(&port->sample_sem);
}

static unsigned int dsm_port_irq_handler(unsigned int irq,struct serialPort* port)
{
    unsigned char iir;
    unsigned char lsr;
    unsigned char msr;

#ifdef DEBUG_ISR
    static int numRSC = 0;
#endif

    for (;;) {
	iir = serial_in(port,UART_IIR) & 0x3f;
#ifdef DEBUG_ISR
	DSMLOG_DEBUG("iir=0x%x\n",iir);
#endif
	if (iir & UART_IIR_NO_INT) break;

	switch (iir) {
	case UART_IIR_THRI:	// 0x02: transmitter holding register empty
#ifdef DEBUG_ISR
	    DSMLOG_DEBUG("UART_IIR_THRI interrupt\n");
#endif
	    transmit_chars(port);
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr,0);
	    break;
	case UART_IIR_RDI:	// 0x04: received data ready
	    lsr = serial_in(port,UART_LSR);
	    receive_chars(port,&lsr,0);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case WIN_COM8_IIR_RDTO:	// 0x0c received data timeout
	    lsr = serial_in(port,UART_LSR);
	    receive_chars(port,&lsr,4);	// 4 character timeout
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case UART_IIR_MSI:	// 0x00 modem status change
#ifdef DEBUG_ISR
	    DSMLOG_DEBUG("UART_IIR_MSI interrupt, MSR=0x%x\n",msr);
#endif
	    msr = serial_in(port, UART_MSR);
	    break;
	case UART_IIR_RLSI:	// 0x06 line status interrupt (error)
#ifdef DEBUG_ISR
	    DSMLOG_DEBUG("UART_IIR_RLSI interrupt, LSR=0x%x\n",lsr);
#endif
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr,0);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case WIN_COM8_IIR_RSC:	// 0x10 XOFF/special character
#ifdef DEBUG_ISR
	    if (!(numRSC++ % 100))
	    	DSMLOG_DEBUG("ISR_RSC interrupt, IIR=0x%x\n",iir);
#endif
	    // received special character
	    lsr = serial_in(port,UART_LSR);
	    receive_chars(port,&lsr,0);
	    break;
	case WIN_COM8_IIR_CTSRTS:	// 0x20 CTS_RTS change
	    msr = serial_in(port, UART_MSR);
#ifdef DEBUG_ISR
	    DSMLOG_DEBUG("ISR_CTSRTS interrupt, MSR=0x%x\n",msr);
#endif
	    break;
	}
    }
    return 0;
}

unsigned int dsm_serial_irq_handler(unsigned int irq,
	void* callbackptr, struct rtl_frame *regs)
{
    struct serialBoard* board = (struct serialBoard*) callbackptr;
    unsigned char bstat;
    int pmask;
    int iport;
    struct serialPort* port;
    int retval = 0;

#ifdef DEBUG_ISR
    DSMLOG_DEBUG("dsm_irq_handler entered\n");
#endif

    // read board interrupt id register
    while ((bstat = serial_inp(board,COM8_BC_IIR)) & board->int_mask) {
#ifdef DEBUG_ISR
	DSMLOG_DEBUG("dsm_irq_handler bstat=0x%x\n", bstat);
#endif
	pmask = 1;
	for (iport = 0; iport < board->numports; iport++) {
	    if ((bstat & pmask)) {
		port = board->ports + iport;

		rtl_spin_lock (&port->lock);
		retval = dsm_port_irq_handler(irq,port);
		rtl_spin_unlock (&port->lock);

		if (retval) goto err;
		bstat &= ~pmask;
	    }
	    if (!bstat) break;	// done
	    pmask <<= 1;
	}
    }
    return 0;
err:
    return retval;
}

/*
 * Return: negative RTL errno
 */
static int rtl_dsm_ser_open(struct rtl_file* filp)
{
    int retval = -RTL_EACCES;
#ifdef DEBUG
    DSMLOG_DEBUG("rtl_dsm_ser_open\n");
#endif
    // if (!(filp->f_flags & RTL_O_NONBLOCK)) return retval;
    struct serialPort* port = (struct serialPort*) filp->f_priv;
    if ((retval = open_port(port)) != 0) return retval;
    return 0;
}

/*
 * Return: negative RTL errno
 */
static int rtl_dsm_ser_release(struct rtl_file* filp)
{
    int retval;
#ifdef DEBUG
    DSMLOG_DEBUG("rtl_dsm_ser_release\n");
#endif
    struct serialPort* port = (struct serialPort*) filp->f_priv;
    if ((retval = close_port(port)) != 0) return retval;
    return 0;
}

/*
 * User read of this device.  
 * Return at most count number of characters.
 * Return: negative RTL errno, or positive number of bytes read.
 */
static rtl_ssize_t rtl_dsm_ser_read(struct rtl_file *filp, char *buf, rtl_size_t count, off_t *pos)
{
    struct serialPort* port = (struct serialPort*) filp->f_priv;
    unsigned long flags;

    rtl_ssize_t retval = 0;
    rtl_ssize_t lout;

    struct rtl_timespec timeout;

    rtl_clock_gettime(RTL_CLOCK_REALTIME,&timeout);
    // rtl_timespec_add_ns(&timeout, port->read_timeout_nsec);
    timeout.tv_sec += port->read_timeout_sec;
    timeout.tv_nsec += port->read_timeout_nsec;
    if (timeout.tv_nsec >= NSECS_PER_SEC) {
	timeout.tv_sec++;
	timeout.tv_nsec -= NSECS_PER_SEC;
    }

    while (count > 0) {
	// write any available samples from 
#ifdef DEBUG
	DSMLOG_DEBUG("dsm_ser_read, count=%d, port->unwrittenl=%d,output cnt=%d\n",
	    count,port->unwrittenl,
	    CIRC_CNT(port->output_samples.head,port->output_samples.tail,OUTPUT_SAMPLE_QUEUE_SIZE));
#endif
	// copy samples to buf, until it's full or no samples left
	for (;;) {
	    // write last partially unwritten sample.
	    if ((lout = port->unwrittenl) > 0) {
		if (lout > count) lout = count;
		memcpy(buf,port->unwrittenp,lout);
		port->unwrittenp += lout;
		port->unwrittenl -= lout;
		count -= lout;
		retval += lout;
		buf += lout;
		// couldn't write all this sample, buffer must be full
		if (port->unwrittenl > 0) return retval;
		port->cosamp->length = 0;	// finished with it
		INCREMENT_TAIL(port->output_samples,OUTPUT_SAMPLE_QUEUE_SIZE);
	    }
	    if (port->output_samples.head == port->output_samples.tail) break;
	    port->cosamp = port->output_samples.buf[port->output_samples.tail];
	    port->unwrittenp = (char*) port->cosamp;
	    port->unwrittenl = SIZEOF_DSM_SAMPLE_HEADER + port->cosamp->length;
	}

	// room left in buf, wait for more data

	/* Note that we don't increment the absolute
	 * time of the timeout again unless no data have arrived.
	 * It is an absolute cut-off time for this read.
	 * We want to return the data in a timely manner -
	 * typical tradeoff between efficiency and responsiveness.
	 */
	if (rtl_sem_timedwait(&port->sample_sem,&timeout) < 0)
	{
	    if (rtl_errno == RTL_EINTR) {
		    DSMLOG_WARNING("dsm_ser_read sem_wait interrupt\n");
		    // if (retval > 0) return retval;
		    return -rtl_errno;
	    }
	    else if (rtl_errno == RTL_ETIMEDOUT) {
		    // if timeout return what we've read
		    if (retval > 0) return retval;

		    // increment timeout if no data
		    timeout.tv_sec += port->read_timeout_sec;
		    timeout.tv_nsec += port->read_timeout_nsec;
		    if (timeout.tv_nsec >= NSECS_PER_SEC) {
			timeout.tv_sec++;
			timeout.tv_nsec -= NSECS_PER_SEC;
		    }
		    continue;
	    }
	    else {
		DSMLOG_ERR("dsm_ser_read sem_wait unknown error: %d\n",
		    rtl_errno);
		return -rtl_errno;
	    }
	}

	/*
	 * We're checking the equality of sample_queue.head and
	 * sample_queue.tail here without spin locking.
	 * If head is not equal to tail then that condition
	 * will not be changed anywhere else but here.
	 * This function is the only one that will set tail
	 * equal to head.  The interrupt function will only 
	 * make them "more unequal".  We only do a spin lock
	 * when we change the value of tail.
	 */

	/*
	 * Only scan one uart sample here
	 */
	if (port->uart_samples.head != port->uart_samples.tail) {
	    struct dsm_sample* samp =
	    	port->uart_samples.buf[port->uart_samples.tail];
#ifdef DEBUG
	    DSMLOG_DEBUG("scanning\n");
#endif

            port->scan_func(port,samp);

	    rtl_spin_lock_irqsave(&port->lock,flags);
	    INCREMENT_TAIL(port->uart_samples,UART_SAMPLE_QUEUE_SIZE);
	    rtl_spin_unlock_irqrestore(&port->lock,flags);
	}
    }

#ifdef DEBUG
    DSMLOG_DEBUG("ser_read, retval=%d\n",retval);
#endif
    return retval;
}

/*
 * Return: negative RTL errno, or positive number of bytes written.
 */
static rtl_ssize_t rtl_dsm_ser_write(struct rtl_file *filp, const char *buf, rtl_size_t count, off_t *pos)
{
    struct serialPort* port = (struct serialPort*) filp->f_priv;

    // DSMLOG_DEBUG("dsm_ser_write, count=%d\n",count);
    int res = queue_transmit_chars(port,buf,count);
    return res;
}

/*
 * Return: negative RTL errno
 */
static int rtl_dsm_ser_ioctl(struct rtl_file *filp, unsigned int request,
	unsigned long arg)
{
    struct serialPort* port = (struct serialPort*) filp->f_priv;
    struct termios* termios;

    int retval = -RTL_EINVAL;
    switch (request) {
    case DSMSER_TCSETS:		/* user set of termios parameters */
	termios = (struct termios*) arg;

#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_TCSETS,port->devname=%s\n",port->devname);

	DSMLOG_DEBUG("sizeof(struct termios)=%d\n",sizeof(struct termios));
	DSMLOG_DEBUG("termios=0x%x\n",termios);
	DSMLOG_DEBUG("c_iflag=0x%x %x\n",
	    &(termios->c_iflag),termios->c_iflag);
	DSMLOG_DEBUG("c_oflag=0x%x %x\n",
	    &(termios->c_oflag),termios->c_oflag);
	DSMLOG_DEBUG("c_cflag=0x%x %x\n",
	    &(termios->c_cflag),termios->c_cflag);
	DSMLOG_DEBUG("c_lflag=0x%x %x\n",
	    &(termios->c_lflag),termios->c_lflag);
	DSMLOG_DEBUG("c_line=0x%x\n", (void *)&(termios->c_line));
	DSMLOG_DEBUG("c_cc=0x%x\n", (void *)&(termios->c_cc[0]));
#endif
	retval = tcsetattr(port,termios);
	break;
    case DSMSER_TCGETS:		/* user get of termios parameters */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_TCGETS\n");
#endif
	retval = tcgetattr(port, (struct termios*) arg);
	break;
    case DSMSER_WEEPROM:	/* write config to eeprom */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_WEEPROM\n");
#endif
	retval = write_eeprom(port->board);
	break;
    case DSMSER_SET_PROMPT:	/* set the prompt for this port */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_SET_PROMPT\n");
#endif
	retval = set_prompt(port,(struct dsm_serial_prompt*)arg);
	break;
    case DSMSER_GET_PROMPT:	/* get the prompt for this port */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_GET_PROMPT\n");
#endif
	retval = get_prompt(port, (struct dsm_serial_prompt*)arg);
	break;
    case DSMSER_START_PROMPTER:	/* start the prompter for this port */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_START_PROMPTER\n");
#endif
	retval = start_prompter(port);
	break;
    case DSMSER_STOP_PROMPTER:	/* stop the prompter for this port */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_STOP_PROMPTER\n");
#endif
	retval = stop_prompter(port);
	break;
    case DSMSER_SET_RECORD_SEP:	/* set the record separator for this port */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_SET_RECORD_SEP\n");
#endif
	retval = set_record_sep(port, (struct dsm_serial_record_info*)arg);
	break;
    case DSMSER_GET_RECORD_SEP:	/* get the record separator for this port */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_GET_RECORD_SEP\n");
#endif
	retval = get_record_sep(port, (struct dsm_serial_record_info*)arg);
	break;
    case DSMSER_GET_STATUS:	/* get the status parameters */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_GET_STATUS\n");
#endif
	retval = get_status(port, (struct dsm_serial_status*)arg);
	break;
    case DSMSER_SET_LATENCY:	/* set the buffering latency, in usecs */
#ifdef DEBUG
	DSMLOG_DEBUG("DSMSER_SET_LATENCY_USEC\n");
#endif
	retval = set_latency_usec(port, *(long*)arg);
	break;
    default:
	DSMLOG_ERR("%s: unknown ioctl cmd\n",devprefix);
	break;
    }
    return retval;
}


static int rtl_dsm_ser_poll_handler(const struct rtl_sigaction *sigact)
{
    DSMLOG_DEBUG("poll_handler called\n");
    return 0;
}

static struct rtl_file_operations rtl_dsm_ser_fops = {
    read:           rtl_dsm_ser_read,
    write:          rtl_dsm_ser_write,
    ioctl:          rtl_dsm_ser_ioctl,
    open:           rtl_dsm_ser_open,
    release:        rtl_dsm_ser_release,
    install_poll_handler: rtl_dsm_ser_poll_handler,
};


/*
 * Exposed function to return number of boards I am configured for.
 */
int dsm_serial_get_numboards()
{
  return numboards;
}

/*
 * Exposed function to return number of ports on a given board
 * where board is in the range 0:(numboards-1)
 */
int dsm_serial_get_numports(int board)
{
    if (board < 0 || board >= numboards) return 0;
    return boardInfo[board].numports;
}

/*
 * Exposed function to return our device prefix.
 */
const char* dsm_serial_get_devprefix()
{
    return devprefix;
}

/*
 * Exposed function to return a device name for a given port
 * number.
 */
const char* dsm_serial_get_devname(int portnum)
{
    int ib,ip;
    for (ib = 0; ib < numboards; ib++) {
        for (ip = 0; ip < boardInfo[ib].numports; ip++) {
	    struct serialPort* port = boardInfo[ib].ports + ip;
	    if (port->portNum == portnum) return port->devname;
	}
    }
    return 0;
}

/*
 * Return: negative Linux (not RTLinux) errno.
 */
int init_module(void)
{
    int ib,ip,i;
    int numirqs;
    int numioport0s;
    int retval = -EINVAL;
    unsigned long addr;
    char devname[128];

    clock_res_msec = get_msec_clock_resolution();
    clock_res_msec_o2 = clock_res_msec / 2;

    // DSM_VERSION_STRING is found in dsm_version.h
    DSMLOG_NOTICE("version: %s\n",DSM_VERSION_STRING);

    /* check board types to see how many boards are configured */
    for (ib = 0; ib < MAX_NUM_BOARDS; ib++)
	if (brdtype[ib] == BOARD_UNKNOWN) break;
    numboards = ib;

    if (numboards == 0) {
	DSMLOG_ERR("No boards configured, all brdtype[]==BOARD_UNKNOWN\n");
	goto err0;
    }

    /* check non-zero irqs */
    for (ib = 0; ib < MAX_NUM_BOARDS; ib++) 
	if (irq_param[ib] == 0) break;
    numirqs = ib;

    if (numirqs != numboards) {
	DSMLOG_ERR("incorrect number of IRQs, should be %d of them\n",
		numboards);
	goto err0;
    }

    /* check non-zero ioport0s */
    for (ib = 0; ib < MAX_NUM_BOARDS; ib++) 
	if (ioport0[ib] == 0) break;
    numioport0s = ib;

    if (numioport0s < numboards)  {
	DSMLOG_ERR("incorrect number of ioport0 addresses, should be at least %d\n",
		numboards);
	goto err0;
    }

    retval = -ENOMEM;
    boardInfo = rtl_gpos_malloc( numboards * sizeof(struct serialBoard) );
    if (!boardInfo) goto err0;

    int portcounter = 0;

    for (ib = 0; ib < numboards; ib++) {

	struct serialBoard *brd = boardInfo + ib;

        memset(brd,0,sizeof(struct serialBoard));

	int boardirq = 0;

	retval = -EBUSY;
	addr = SYSTEM_ISA_IOPORT_BASE + ioport[ib];
	if (check_region(addr, 8)) {
	    DSMLOG_ERR("ioports at 0x%x already in use\n",addr);
	    goto err1;
	}
	request_region(addr, 8, "dsm_serial");
	brd->addr = addr;

	retval = -EINVAL;
	brd->type = brdtype[ib];
	switch (brd->type) {
	case BOARD_WIN_COM8:
	    brd->numports = 8;
	    break;
	default:
	    DSMLOG_ERR("unknown board type: %d\n",brd->type);
	    goto err1;
	}
#ifdef DEBUG
	DSMLOG_DEBUG("numports=%d\n",brd->numports);
#endif

	retval = -ENOMEM;
	brd->ports = rtl_gpos_malloc(
          brd->numports * sizeof(struct serialPort) );
	if (!brd->ports) goto err1;

	for (ip = 0; ip < brd->numports; ip++) {
	    struct serialPort* port = brd->ports + ip;
	    memset(port,0,sizeof(struct serialPort));
	    init_serialPort_struct(port);
	    port->board = brd;
	}

	for (ip = 0; ip < brd->numports; ip++) {
	    struct serialPort* port = brd->ports + ip;

	    port->portNum = portcounter;
	    port->portIndex = ip;
	    port->ioport = ioport0[ib] + (ip * 8);

	    addr = SYSTEM_ISA_IOPORT_BASE + port->ioport;
	    retval = -EBUSY;
	    if (check_region(addr, 8)) {
		DSMLOG_ERR("ioports at 0x%x already in use\n",
			addr);
		goto err1;
	    }
	    request_region(addr, 8, "dsm_serial");
	    port->addr = addr;

	    retval = -ENOMEM;
	    port->xmit.buf = rtl_gpos_malloc( SERIAL_XMIT_SIZE );
	    if (!port->xmit.buf) goto err1;

	    port->uart_samples.buf = rtl_gpos_malloc(UART_SAMPLE_QUEUE_SIZE *
	    	sizeof(void*) );
	    if (!port->uart_samples.buf) goto err1;
	    memset(port->uart_samples.buf,0,
	    	UART_SAMPLE_QUEUE_SIZE *sizeof(void*) );

	    for (i = 0; i < UART_SAMPLE_QUEUE_SIZE; i++) {
		struct dsm_sample* samp = (struct dsm_sample*)
		  rtl_gpos_malloc( SIZEOF_DSM_SAMPLE_HEADER +
				   UART_SAMPLE_SIZE );
		if (!samp) goto err1;
		memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
				   UART_SAMPLE_SIZE );
		port->uart_samples.buf[i] = samp;
	    }

	    port->output_samples.buf =
	    	rtl_gpos_malloc(OUTPUT_SAMPLE_QUEUE_SIZE *
		    sizeof(void*) );
	    if (!port->output_samples.buf) goto err1;
	    memset(port->output_samples.buf,0,
	    	OUTPUT_SAMPLE_QUEUE_SIZE *sizeof(void*) );

	    for (i = 0; i < OUTPUT_SAMPLE_QUEUE_SIZE; i++) {
		struct dsm_sample* samp = (struct dsm_sample*)
		  rtl_gpos_malloc( SIZEOF_DSM_SAMPLE_HEADER +
				   MAX_DSM_SERIAL_SAMPLE_SIZE );
		if (!samp) goto err1;
		memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
				   MAX_DSM_SERIAL_SAMPLE_SIZE );
		port->output_samples.buf[i] = samp;
	    }

	    port->irq = irq_param[ib];

	    if (boardirq == 0) boardirq = port->irq;
	    retval = -EINVAL;
	    if (boardirq != port->irq) {
	        DSMLOG_ERR("current version only supports one IRQ per board\n");
		goto err1;
	    }

	    /* There are the beginnings of support here for multiple
	     * IRQs per board, but we don't actually support
	     * that yet.
	     */
	    if (numirqs == numboards) port->irq = irq_param[ib];
	    else port->irq = irq_param[portcounter];

	    if (boardirq == 0) boardirq = port->irq;
	    retval = -EINVAL;
	    if (boardirq != port->irq) {
	        DSMLOG_ERR("current version only supports one IRQ per board\n");
		goto err1;
	    }

	    switch (brd->type) {
	    case BOARD_WIN_COM8:
#ifdef DEBUG
		/* read ioport address and irqs for ports on board */
		serial_out(brd,COM8_BC_IR,ip);
		unsigned char x;
		x = serial_in(brd,COM8_BC_BAR);
		DSMLOG_DEBUG("addr=0x%x, enabled=0x%x\n",
		    (x & 0x7f) << 3,(x & 0x80));
		DSMLOG_DEBUG("irq=%d\n",
		    serial_in(brd,COM8_BC_IAR) & 0xf);
#endif
		
		/* configure ioport address and irqs for ports on board */
		serial_out(brd,COM8_BC_IR,ip);
		/* must enable uart for it to respond at this address */
		serial_out(brd,COM8_BC_BAR,(port->ioport >> 3) +
		    COM8_BC_UART_ENABLE);
		serial_out(brd,COM8_BC_IAR,port->irq);

#ifdef DEBUG
		/* read ioport address and irqs for ports on board */
		serial_out(brd,COM8_BC_IR,ip);
		x = serial_in(brd,COM8_BC_BAR);
		DSMLOG_DEBUG("addr=0x%x, enabled=0x%x\n",
		    (x & 0x7f) << 3,(x & 0x80));
		DSMLOG_DEBUG("irq=%d\n",
		    serial_in(brd,COM8_BC_IAR) & 0xf);
#endif
		break;
	    }

	    retval = -ENOMEM;
	    sprintf(devname, "%s/%s%d", getDevDir(),devprefix,portcounter);
	    port->devname = (char *) rtl_gpos_malloc( strlen(devname) + 1 );
	    if (!port->devname) goto err1;
	    strcpy(port->devname,devname);

	    if ( rtl_register_dev(devname, &rtl_dsm_ser_fops,(unsigned long)port) ) {
		DSMLOG_ERR("Unable to register %s device\n",devname);
		/* if port->devname is non-zero then it has been registered */
		rtl_gpos_free(port->devname);
		port->devname = 0;
		retval = -EIO;
		goto err1;
	    }
	    portcounter++;
	}

	switch (brd->type) {
	case BOARD_WIN_COM8:
	    if ((retval = rtl_request_isa_irq(boardirq,dsm_serial_irq_handler,
		brd)) < 0) goto err1;
	    break;
	}
	brd->irq = boardirq;
    }

    return 0;

err1:
    if (boardInfo) {
	for (ib = 0; ib < numboards; ib++) {
	    struct serialBoard *brd = boardInfo + ib;
	    if (brd->irq) rtl_free_isa_irq(brd->irq);
	    brd->irq = 0;

	    if (brd->addr) release_region(brd->addr, 8);
	    brd->addr = 0;

	    if (brd->ports) {
		for (ip = 0; ip < brd->numports; ip++) {
		    struct serialPort* port = brd->ports + ip;

		    if (port->addr) release_region(port->addr, 8);
		    port->addr = 0;
		    if (port->xmit.buf) rtl_gpos_free(port->xmit.buf);
		    port->xmit.buf = 0;

		    if (port->uart_samples.buf) {
			for (i = 0; i < UART_SAMPLE_QUEUE_SIZE; i++)
			  if (port->uart_samples.buf[i])
			      rtl_gpos_free(port->uart_samples.buf[i]);
			rtl_gpos_free(port->uart_samples.buf);
			port->uart_samples.buf = 0;
		    }

		    if (port->output_samples.buf) {
			for (i = 0; i < OUTPUT_SAMPLE_QUEUE_SIZE; i++)
			  if (port->output_samples.buf[i])
			      rtl_gpos_free(port->output_samples.buf[i]);
			rtl_gpos_free(port->output_samples.buf);
			port->output_samples.buf = 0;
		    }

		    rtl_sem_destroy(&port->sample_sem);

		    if (port->devname) {
			rtl_unregister_dev(port->devname);
			rtl_gpos_free(port->devname);
			port->devname = 0;
		    }
		}
		rtl_gpos_free(brd->ports);
		brd->ports = 0;
	    }
	}

	rtl_gpos_free(boardInfo);
	boardInfo = 0;
    }
err0:
    return retval;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
#ifdef DEBUG
    DSMLOG_DEBUG("starting\n");
#endif
    int ib, ip,i;
    for (ib = 0; ib < numboards; ib++) {
	if (boardInfo[ib].irq) rtl_free_isa_irq(boardInfo[ib].irq);
	boardInfo[ib].irq = 0;

	if (boardInfo[ib].addr) release_region(boardInfo[ib].addr, 8);
	boardInfo[ib].addr = 0;

	if (boardInfo[ib].ports) {
	    for (ip = 0; ip < boardInfo[ib].numports; ip++) {
		struct serialPort* port = boardInfo[ib].ports + ip;

		close_port(port);
		if (port->addr) release_region(port->addr, 8);
		port->addr = 0;
		if (port->xmit.buf) rtl_gpos_free(port->xmit.buf);
		port->xmit.buf = 0;

		if (port->uart_samples.buf) {
		    for (i = 0; i < UART_SAMPLE_QUEUE_SIZE; i++)
		      if (port->uart_samples.buf[i])
			  rtl_gpos_free(port->uart_samples.buf[i]);
		    rtl_gpos_free(port->uart_samples.buf);
		    port->uart_samples.buf = 0;
		}

		if (port->output_samples.buf) {
		    for (i = 0; i < OUTPUT_SAMPLE_QUEUE_SIZE; i++)
		      if (port->output_samples.buf[i])
			  rtl_gpos_free(port->output_samples.buf[i]);
		    rtl_gpos_free(port->output_samples.buf);
		    port->output_samples.buf = 0;
		}

		rtl_sem_destroy(&port->sample_sem);

		if (port->devname) {
#ifdef DEBUG
		    DSMLOG_DEBUG("rtl_unregister_dev: %s\n",port->devname);
#endif
		    rtl_unregister_dev(port->devname);
		    rtl_gpos_free(port->devname);
		    port->devname = 0;
		}
	    }
	    rtl_gpos_free(boardInfo[ib].ports);
	    boardInfo[ib].ports = 0;
	}
    }

    rtl_gpos_free(boardInfo);
    boardInfo = 0;
    DSMLOG_NOTICE("done\n");
}

