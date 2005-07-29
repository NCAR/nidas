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

#include <dsm_serial.h>
#include <dsm_viper.h>
#include <rtl_isa_irq.h>
#include <irigclock.h>
#include <win_com8.h>

// #define SERIAL_DEBUG_AUTOCONF

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
static int irqs[MAX_NUM_BOARDS] = { 11,0,0 };
MODULE_PARM(irqs, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(irqs, "IRQ number");

static struct serialBoard *boardInfo = 0;

unsigned int dsm_serial_irq_handler(unsigned int irq,
	void* callbackptr, struct rtl_frame *regs);

static unsigned int dsm_port_irq_handler(unsigned int irq,struct serialPort* port);

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
	rtl_printf("get_baud_rate, i=%d\n",i);
#endif
        if (i & CBAUDEX) {
#ifdef DEBUG
		rtl_printf("get_baud_rate, CBAUDEX\n");
#endif
                i &= ~CBAUDEX;
                if (i < 1 || i+15 >= n_baud_table)
                        termios->c_cflag &= ~CBAUDEX;
                else
                        i += 15;
        }
#ifdef DEBUG
	rtl_printf("get_baud_rate, i=%d\n",i);
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
	    if (scratch == 0x10)
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
    rtl_printf("Testing dsmser%d (0x%04lx)...\n", port->portNum,
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
	rtl_printf("serial: dsmser%d: simple autoconfig failed "
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
	rtl_printf("serial: dsmser%d: no UART loopback failed\n",
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
    rtl_printf("cleared FIFO\n");
    serial_outp(port, UART_FCR, 0);
    rtl_printf("disabled FIFO\n");
    (void)serial_in(port, UART_RX);
    serial_outp(port, UART_IER, 0);
									    
    rtl_spin_unlock_irqrestore(&port->lock,flags);

    rtl_printf("uart is a %s\n",uart_config[port->type].name);
#ifdef DEBUG
#endif
    return 0;
}

/*
 * Taken from linux drivers/char/serial.c change_speed
 * Return: negative RTL errno
 */
static int change_speed(struct serialPort* port, struct termios* termios)
{
    int quot = 0, baud_base, baud;
    unsigned cflag,cval,fcr=0;
    int bits;
    unsigned long flags;

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
    rtl_printf("baud=%d\n",baud);
#endif
    if (!baud)
	baud = 9600;    /* B0 transition handled in rs_set_termios */

    baud_base = port->baud_base;

    rtl_spin_lock_irqsave(&port->lock,flags);

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
    rtl_printf("baud=%d, quot=%d, 0x%x\n",baud,quot,quot);
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
	rtl_printf("set FCR\n");
    }

    memcpy(&port->termios,termios,sizeof(struct termios));

    rtl_spin_unlock_irqrestore(&port->lock,flags);

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

    rtl_spin_lock_irqsave (&port->lock,flags);

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
	    rtl_printf("uart_startup: cleared FIFO\n");
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
	    rtl_printf("ttyS%d: LSR safety check engaged!\n", port->portNum);
	    retval = -RTL_ENODEV;
	    goto errout;
    }
                                                                                
    /*
     * Now, initialize the UART
     */
    serial_outp(port, UART_LCR, UART_LCR_WLEN8);    /* reset DLAB */

    // port->MCR |= ALPHA_KLUDGE_MCR;          /* Don't ask */
    serial_outp(port, UART_MCR, port->MCR);

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

    rtl_spin_unlock_irqrestore(&port->lock,flags);


errout:
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
    rtl_printf("prompt=\"%s\", len=%d, rate=%d\n",
    	port->prompt.str,port->prompt.len,(int)port->prompt.rate);
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
	port->output_char_overflows += (port->prompt.len - res);
    }
#ifdef DEBUG
    rtl_printf("queue_transmit_chars %s len=%d, res=%d\n",
		    port->prompt.str,port->prompt.len,res);
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

/*
 * Return: negative RTL errno
 */
static int set_record_sep(struct serialPort* port,
	struct dsm_serial_record_info* sep)
{
    unsigned long flags;
    rtl_spin_lock_irqsave (&port->lock,flags);
    memcpy(&port->recinfo,sep,sizeof(struct dsm_serial_record_info));

    port->sepcnt = 0;
    port->incount = 0;

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
    rtl_printf("pe_cnt=%d,oe_cnt=%d\n",port->pe_cnt,port->oe_cnt);
#endif

    status->pe_cnt = port->pe_cnt;
    status->oe_cnt = port->oe_cnt;
    status->fe_cnt = port->fe_cnt;
    status->input_char_overflows = port->input_char_overflows;
    status->output_char_overflows = port->output_char_overflows;
    status->sample_overflows = port->sample_overflows;
    status->nsamples = port->nsamples;

    status->char_transmit_queue_length = CIRC_CNT(port->xmit.head,
    	port->xmit.tail,SERIAL_XMIT_SIZE);
    status->char_transmit_queue_size = SERIAL_XMIT_SIZE;

    status->sample_queue_length = CIRC_CNT(port->sample_queue.head,
    	port->sample_queue.tail,SAMPLE_QUEUE_SIZE);
    status->sample_queue_size = SAMPLE_QUEUE_SIZE;

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
    rtl_printf("open_port, uart=%s\n",uart_config[port->type]);
#endif

    retval = uart_startup(port);

    rtl_spin_lock_irqsave(&port->board->lock,flags);
    port->board->int_mask |= (1 << port->portIndex);
    rtl_spin_lock_irqsave(&port->board->lock,flags);

    rtl_spin_lock_irqsave(&port->lock,flags);
    port->pe_cnt = 0;
    port->oe_cnt = 0;
    port->fe_cnt = 0;
    port->input_char_overflows = 0;
    port->output_char_overflows = 0;
    port->sample_overflows = 0;
    port->nsamples = 0;
    port->incount = port->sepcnt = 0;
    port->xmit.head = port->xmit.tail = 0;
    port->sample_queue.head = port->sample_queue.tail = 0;
    port->sample = port->sample_queue.buf[port->sample_queue.head];
    port->bom_timetag = UNKNOWN_TIMETAG_VALUE;
    port->unwrittenp = 0;
    port->unwrittenl = 0;
    rtl_spin_unlock_irqrestore(&port->lock,flags);

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
      rtl_printf("enable EEPROM write failed: timeout\n");
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
	  rtl_printf("writing config for port %d to EEPROM failed: timeout\n",
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
      rtl_printf("enable EEPROM write failed: timeout\n");
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
    port->portNum = 0;
    port->portIndex = 0;
    port->ioport = 0;
    port->addr = 0;
    port->irq = 0;
    port->type = PORT_UNKNOWN;
    rtl_spin_lock_init (&port->lock);
    port->flags = 0;
    port->revision = 0;
    port->MCR = port->IER = port->LCR = port->ACR = 0;
    port->baud_base = 115200;
    port->quot = 0;
    port->custom_divisor = 0;
    port->timeout = 0;
    port->xmit_fifo_size = 1;

    port->prompt.str[0] = '\0';
    port->prompt.len = 0;
    port->prompt.rate = 0;
    port->promptOn = 0;

    port->recinfo.sep[0] = '\0';
    port->recinfo.sepLen = 0;
    port->recinfo.atEOM = 1;
    port->recinfo.recordLen = 0;

    port->sepcnt = 0;

    port->sample_queue.buf = 0;
    port->sample_queue.head = port->sample_queue.tail = 0;
    port->sample = 0;
    port->unwrittenp = 0;
    port->unwrittenl = 0;
    port->bom_timetag = UNKNOWN_TIMETAG_VALUE;

    port->nsamples = 0;

    /* semaphore timeout in read method */
    /*
     * May want to make this read timeout settable from user space
     * with an ioctl.  Or one could set it equal to the
     * prompt rate, but that wouldn't account for non-prompted
     * sensors. 
     */
    port->read_timeout_nsec = 250000000;	// 250 msec = 1/4 sec

    rtl_sem_init(&port->sample_sem,0,0);

    port->xmit.buf = 0;
    port->xmit.head = port->xmit.tail = 0;

    port->pe_cnt = port->oe_cnt = port->fe_cnt =
    	port->input_char_overflows = port->output_char_overflows = 0;
    port->sample_overflows = 0;

    rtl_memset(&port->termios, 0, sizeof(struct termios));
    port->termios.c_cflag = B38400 | CS8 | HUPCL;
}

/*
 * An input_char_overflow occurs when the parser cannot find a
 * record separator in the data and the input characters
 * overflow the available sample buffer.  This could be due
 * to wrong parity, wrong baud rate, mis-configured sensor
 * etc.
 */
static void inc_input_char_overflow(struct serialPort* port)
{
}

/*
 * Note: this is called from interrupt code.
 * We're finished filling the sample at the head of the sample_queue,
 * and it is ready to be sent on. Increment the head, post
 * the semaphore, and update port->sample so that it points
 * to the new head sample.
 */
static void post_sample(struct serialPort* port)
{
    int n;
    if (port->incount == 0 || port->sample->timetag == UNKNOWN_TIMETAG_VALUE)
    	return;

    // port->lock is already locked

    /* compute how many samples in the queue are available to be
     * written to.  If the reader of this device has fallen behind
     * then this number will be zero, in which case we
     * increment the sample_overflows counter and over-write
     * the current sample with new data - i.e. the sample
     * is lost.
     */
    n = CIRC_SPACE(port->sample_queue.head,port->sample_queue.tail,
    	SAMPLE_QUEUE_SIZE);
    if (n > 0) {
	port->sample->length = (dsm_sample_length_t) port->incount;

	/* increment head */
	port->sample_queue.head = (port->sample_queue.head + 1) &
		(SAMPLE_QUEUE_SIZE-1);
	rtl_sem_post(&port->sample_sem);
	port->nsamples++;

	port->sample = port->sample_queue.buf[port->sample_queue.head];
    }
    else port->sample_overflows++;
    port->sample->timetag = UNKNOWN_TIMETAG_VALUE;
}

/*
 * insert character c into a sample. Recognize samples by
 * a beginning of message separator.  If we recognize
 * a record sample, send it to the FIFO.
 */
static void add_char_sep_bom(struct serialPort* port, unsigned char c)
{
    // initial conditions:
    //    sepcnt = 0;
    //    incount = 0;

    if (port->sepcnt < port->recinfo.sepLen) {
	// This block is entered if we are currently scanning
	// for the message separator at the beginning of the message.
        if (c == port->recinfo.sep[port->sepcnt]) {
	    // We now have a character match to the record separator.
	    // increment the separator counter.
	    // first character: grab clock for next message,
	    if (port->sepcnt++ == 0) port->bom_timetag = GET_MSEC_CLOCK;
	    // if matched entire separator string send previous sample
	    if (port->sepcnt == port->recinfo.sepLen) {

		// send previous sample
		if (port->incount > 0) post_sample(port);

		// setup current sample
		port->sample->timetag = port->bom_timetag;
		port->incount = 0;
		// copy separator to next sample
		memcpy(port->sample->data,port->recinfo.sep,
			port->sepcnt);
		port->incount += port->sepcnt;
		// leave sepcnt equal to recinfo.sepLen
	    }
	}
	else {
	    // At this point:
	    // 1. we're expecting the BOM message separator
	    // 2. the current character fails a match with the BOM string
	    // We'll send the faulty data along anyway so that
	    // the user can see what is going on.
	    // If we have mistaken some characters for the 
	    // separator, copy them to the sample data.
	    if (port->sepcnt > 0) {	// previous partial match
		if (port->incount + port->sepcnt >
			MAX_DSM_SERIAL_MESSAGE_SIZE) {
		    post_sample(port);		// faulty sample
		    port->input_char_overflows++;
		    port->incount = 0;
		}
		memcpy(port->sample->data + port->incount,
			port->recinfo.sep, port->sepcnt);
		port->incount += port->sepcnt;
		port->sepcnt = 0;
		// try matching first character of separator
		// this isn't infinitely recursive because now sepcnt=0
		add_char_sep_bom(port,c);
		return;
	    }
	    // at this point sepcnt == 0 but we don't have a match of the
	    // first character.  This could only happen for the
	    // first record, or if we had a previous partial
	    // match of the BOM string, or an overflow.
	    if (port->incount == MAX_DSM_SERIAL_MESSAGE_SIZE) {
		post_sample(port);
		port->input_char_overflows++;
		port->incount = 0;
	    }
	    if (port->incount == 0)
		port->bom_timetag = GET_MSEC_CLOCK;

	    port->sample->timetag = port->bom_timetag;
	    port->sample->data[port->incount++] = c;
	}
    }
    else {
	// At this point we have a match to the BOM separater string.
	// and are filling the data buffer.

	// if a variable record length, then check to see
	// if the character matches the initial character
	// in the separator string.
	if (port->recinfo.recordLen == 0) {
	    if (c == port->recinfo.sep[0]) {
	        port->sepcnt = 0;
		// recursive call
		add_char_sep_bom(port,c);
		return;
	    }
	    else {
		if (port->incount == MAX_DSM_SERIAL_MESSAGE_SIZE) {
		    post_sample(port);
		    port->input_char_overflows++;
		    port->sample->timetag = GET_MSEC_CLOCK;
		    port->incount = 0;
		    port->sepcnt = 0;
		}
		port->sample->data[port->incount++] = c;
	    }
	}
	else {
	    // fixed record length, save character in buffer.
	    // no chance for overflow here, as long as
	    // recordLen < sizeof(buffer)
	    port->sample->data[port->incount++] = c;
	    // If we're done, scan for separator next.
	    if (port->incount == port->recinfo.recordLen) port->sepcnt = 0;
	}
    }
}

static void add_char_sep_eom(struct serialPort* port, unsigned char c)
{
    // initial conditions:
    //    sepcnt = 0;
    //    incount = 0;

    if (port->incount == MAX_DSM_SERIAL_MESSAGE_SIZE) {
	// send this bogus sample on
	post_sample(port);
	port->input_char_overflows++;
	port->incount = 0;
	port->sepcnt = 0;
    }

    if (port->incount == 0) port->sample->timetag = GET_MSEC_CLOCK;
    port->sample->data[port->incount++] = c;

    if (port->recinfo.recordLen == 0) {
	// if a variable record length, then check to see
	// if the character matches the current character
	// in the end of message separator string.
	if (c == port->recinfo.sep[port->sepcnt]) {
	    // character matched
	    if (++port->sepcnt == port->recinfo.sepLen) {
		post_sample(port);
		port->incount = 0;
		port->sepcnt = 0;
	    }
	}
	else {
	    // variable record length, no match of current character
	    // to EOM string.

	    // go back and check for match at beginning of separator string
	    if (port->sepcnt > 0 && c == port->recinfo.sep[port->sepcnt = 0]) {
		// if a match, do a recursive call
		add_char_sep_eom(port,c);
		return;
	    }
	}
    }
    else {
	// fixed length record
	if (port->incount >= port->recinfo.recordLen) {
	    // we've read in all our characters, now scan separator string
	    // if no match, check from beginning of separator string
	    if (c == port->recinfo.sep[port->sepcnt] ||
		c == port->recinfo.sep[port->sepcnt = 0]) {
		if (++port->sepcnt == port->recinfo.sepLen) {
		    post_sample(port);
		    port->incount = 0;	// matched string
		    port->sepcnt = 0;
		}
	    }
	}
    }
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

void inline check_lsr(struct serialPort*port, unsigned char lsr)
{
    if (lsr & (UART_LSR_PE | UART_LSR_OE | UART_LSR_FE)) {
	if (lsr & UART_LSR_PE) port->pe_cnt++;	// parity errors
	if (lsr & UART_LSR_OE) port->oe_cnt++;	// input overflows
	if (lsr & UART_LSR_FE) port->fe_cnt++;	// framing errors
	// there is also a FIFO bit error here, not sure what
	// to do with it at this point.
    }
}

static inline void receive_chars(struct serialPort* port,unsigned char* lsrp)
{
    // Read fifo until empty
    unsigned char c;
    while(*lsrp & UART_LSR_DR) {
	c = serial_in(port,UART_RX); // Read character
	if (port->recinfo.atEOM) add_char_sep_eom(port,c);
	else add_char_sep_bom(port,c);
	check_lsr(port,*lsrp);
	*lsrp = serial_in(port,UART_LSR);
    }
}

static unsigned int dsm_port_irq_handler(unsigned int irq,struct serialPort* port)
{
    unsigned char iir;
    unsigned char lsr;
    unsigned char msr;
    for (;;) {
	iir = serial_in(port,UART_IIR) & 0x3f;
#ifdef DEBUG
	rtl_printf("iir=0x%x\n",iir);
#endif
	if (iir & UART_IIR_NO_INT) break;
	msr = serial_in(port, UART_MSR);

	switch (iir) {
	case UART_IIR_THRI:	// 0x02: transmitter holding register empty
#ifdef DEBUG
	    rtl_printf("UART_IIR_THRI interrupt\n");
#endif
	    transmit_chars(port);
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr);
	    break;
	case UART_IIR_RDI:	// 0x04: received data ready
	case WIN_COM8_IIR_RDTO:	// 0x0c received data timeout
#ifdef DEBUG
	    rtl_printf("UART_IIR_RDI interrupt\n");
#endif
	    lsr = serial_in(port,UART_LSR);
	    receive_chars(port,&lsr);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case UART_IIR_MSI:	// 0x00 modem status change
	    rtl_printf("UART_IIR_MSI interrupt, MSR=0x%x\n",msr);
	    /* ignore for now */
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case UART_IIR_RLSI:	// 0x06 line status interrupt (error)
	    rtl_printf("UART_IIR_RLSI interrupt, LSR=0x%x\n",lsr);
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case WIN_COM8_IIR_RSC:	// 0x10 XOFF/special character
	    rtl_printf("ISR_RSC interrupt, IIR=0x%x\n",iir);
	    // do we read this character now?
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
	    break;
	case WIN_COM8_IIR_CTSRTS:	// 0x20 CTS_RTS change
	    rtl_printf("ISR_CTSRTS interrupt, MSR=0x%x\n",msr);
	    lsr = serial_in(port,UART_LSR);
	    if (lsr & UART_LSR_DR) receive_chars(port,&lsr);
	    if (lsr & UART_LSR_THRE) transmit_chars(port);
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

#ifdef DEBUG
    rtl_printf("dsm_irq_handler entered\n");
#endif

    // read board interrupt id register
    while ((bstat = serial_inp(board,COM8_BC_IIR)) & board->int_mask) {
#ifdef DEBUG
	rtl_printf("dsm_irq_handler bstat=0x%x\n", bstat);
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
    rtl_printf("rtl_dsm_ser_open\n");
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
    rtl_printf("rtl_dsm_ser_release\n");
    struct serialPort* port = (struct serialPort*) filp->f_priv;
    if ((retval = close_port(port)) != 0) return retval;
    return 0;
}

/*
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
    timeout.tv_nsec += port->read_timeout_nsec;
    if (timeout.tv_nsec >= NSECS_PER_SEC) {
	timeout.tv_sec++;
	timeout.tv_nsec -= NSECS_PER_SEC;
    }

    while (count > 0) {
	if ((lout = port->unwrittenl) > 0) {
	    if (lout > count) lout = count;

	    memcpy(buf,port->unwrittenp,lout);
	    port->unwrittenp += lout;
	    port->unwrittenl -= lout;
	    count -= lout;
	    retval += lout;
	    buf += lout;
	    // finished with this sample
	    if (port->unwrittenl == 0) {
		rtl_spin_lock_irqsave(&port->lock,flags);
		port->sample_queue.tail = (port->sample_queue.tail + 1) &
		    (SAMPLE_QUEUE_SIZE-1);
		rtl_spin_unlock_irqrestore(&port->lock,flags);
	    }
	}
	else {
	    struct dsm_sample* samp = 0;

	    /* Note that we don't increment the absolute
	     * time of the timeout again unless no data have arrived.
	     * It is an absolute cut-off time for this read.
	     * We want to return the data in a timely manner -
	     * typical tradeoff between efficiency and responsiveness.
	     */

	    if (rtl_sem_timedwait(&port->sample_sem,&timeout) < 0)
	    {
		if (rtl_errno == RTL_EINTR) {
			rtl_printf("dsm_ser_read sem_wait interrupt\n");
			return -rtl_errno;
		}
	        else if (rtl_errno == RTL_ETIMEDOUT) {
			// if timeout return what we've read
			if (retval > 0) return retval;

			// increment timeout if no data
			timeout.tv_nsec += port->read_timeout_nsec;
			if (timeout.tv_nsec >= NSECS_PER_SEC) {
			    timeout.tv_sec++;
			    timeout.tv_nsec -= NSECS_PER_SEC;
			}
		}
		else {
		    rtl_printf("dsm_ser_read sem_wait unknown error: %d\n",
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

	    if (port->sample_queue.head != port->sample_queue.tail)
		samp = port->sample_queue.buf[port->sample_queue.tail];

	    if (!samp) continue;
	    port->unwrittenp = (char*) samp;
	    port->unwrittenl =
	      	SIZEOF_DSM_SAMPLE_HEADER + samp->length;
	}
    }

#ifdef DEBUG
    rtl_printf("ser_read, retval=%d\n",retval);
#endif
    return retval;
}

/*
 * Return: negative RTL errno, or positive number of bytes written.
 */
static rtl_ssize_t rtl_dsm_ser_write(struct rtl_file *filp, const char *buf, rtl_size_t count, off_t *pos)
{
    struct serialPort* port = (struct serialPort*) filp->f_priv;

    rtl_printf("dsm_ser_write, count=%d\n",count);

    int togglePrompter = port->promptOn;
    
    if (togglePrompter) stop_prompter(port);
    int res = queue_transmit_chars(port,buf,count);
    if (togglePrompter) start_prompter(port);
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
#ifdef DEBUG
	rtl_printf("DSMSER_TCSETS\n");
#endif
	termios = (struct termios*) arg;
#ifdef DEBUG
	rtl_printf("sizeof(struct termios)=%d\n",sizeof(struct termios));
	rtl_printf("termios=0x%x\n",termios);
	rtl_printf("c_iflag=0x%x %x\n",
	    &(termios->c_iflag),termios->c_iflag);
	rtl_printf("c_oflag=0x%x %x\n",
	    &(termios->c_oflag),termios->c_oflag);
	rtl_printf("c_cflag=0x%x %x\n",
	    &(termios->c_cflag),termios->c_cflag);
	rtl_printf("c_lflag=0x%x %x\n",
	    &(termios->c_lflag),termios->c_lflag);
	rtl_printf("c_line=0x%x\n", (void *)&(termios->c_line));
	rtl_printf("c_cc=0x%x\n", (void *)&(termios->c_cc[0]));
#endif
	retval = change_speed(port,termios);
	break;
    case DSMSER_TCGETS:		/* user get of termios parameters */
#ifdef DEBUG
	rtl_printf("DSMSER_TCGETS\n");
#endif
	retval = tcgetattr(port, (struct termios*) arg);
	break;
    case DSMSER_WEEPROM:	/* write config to eeprom */
#ifdef DEBUG
	rtl_printf("DSMSER_WEEPROM\n");
#endif
	retval = write_eeprom(port->board);
	break;
    case DSMSER_SET_PROMPT:	/* set the prompt for this port */
#ifdef DEBUG
	rtl_printf("DSMSER_SET_PROMPT\n");
#endif
	retval = set_prompt(port,(struct dsm_serial_prompt*)arg);
	break;
    case DSMSER_GET_PROMPT:	/* get the prompt for this port */
#ifdef DEBUG
	rtl_printf("DSMSER_GET_PROMPT\n");
#endif
	retval = get_prompt(port, (struct dsm_serial_prompt*)arg);
	break;
    case DSMSER_START_PROMPTER:	/* start the prompter for this port */
#ifdef DEBUG
	rtl_printf("DSMSER_START_PROMPTER\n");
#endif
	retval = start_prompter(port);
	break;
    case DSMSER_STOP_PROMPTER:	/* stop the prompter for this port */
#ifdef DEBUG
	rtl_printf("DSMSER_STOP_PROMPTER\n");
#endif
	retval = stop_prompter(port);
	break;
    case DSMSER_SET_RECORD_SEP:	/* set the prompt for this port */
#ifdef DEBUG
	rtl_printf("DSMSER_SET_RECORD_SEP\n");
#endif
	retval = set_record_sep(port, (struct dsm_serial_record_info*)arg);
	break;
    case DSMSER_GET_RECORD_SEP:	/* get the prompt for this port */
#ifdef DEBUG
	rtl_printf("DSMSER_GET_RECORD_SEP\n");
#endif
	retval = get_record_sep(port, (struct dsm_serial_record_info*)arg);
	break;
    case DSMSER_GET_STATUS:	/* get the status parameters */
#ifdef DEBUG
	rtl_printf("DSMSER_GET_STATUS\n");
#endif
	retval = get_status(port, (struct dsm_serial_status*)arg);
	break;
    default:
	rtl_printf("%s: unknown ioctl cmd\n",devprefix);
	break;
    }
    return retval;
}


static int rtl_dsm_ser_poll_handler(const struct rtl_sigaction *sigact)
{
    rtl_printf("poll_handler called\n");
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
 * Exposed function to return number of ports on a given board
 * where board is in the range 0:(numboards-1)
 */
const char* dsm_serial_get_devprefix()
{
    return devprefix;
}

/*
 * Exposed function to return number of ports on a given board
 * where board is in the range 0:(numboards-1)
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

    rtl_printf("(%s) %s: compiled on %s at %s\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

    /* check board types to see how many boards are configured */
    for (ib = 0; ib < MAX_NUM_BOARDS; ib++)
	if (brdtype[ib] == BOARD_UNKNOWN) break;
    numboards = ib;

    if (numboards == 0) {
	rtl_printf("No boards configured, all brdtype[]==BOARD_UNKNOWN\n");
	goto err0;
    }

    /* check non-zero irqs */
    for (ib = 0; ib < MAX_NUM_BOARDS; ib++) 
	if (irqs[ib] == 0) break;
    numirqs = ib;

    if (numirqs != numboards) {
	rtl_printf("incorrect number of IRQs, should be %d of them\n",
		numboards);
	goto err0;
    }

    /* check non-zero ioport0s */
    for (ib = 0; ib < MAX_NUM_BOARDS; ib++) 
	if (ioport0[ib] == 0) break;
    numioport0s = ib;

    if (numioport0s < numboards)  {
	rtl_printf("incorrect number of ioport0 addresses, should be at least %d\n",
		numboards);
	goto err0;
    }

    retval = -ENOMEM;
    boardInfo = rtl_gpos_malloc( numboards * sizeof(struct serialBoard) );
    if (!boardInfo) goto err0;
    for (ib = 0; ib < numboards; ib++) {
	boardInfo[ib].type = brdtype[ib];
	boardInfo[ib].addr = 0;
	boardInfo[ib].irq = 0;
	boardInfo[ib].ports = 0;
	boardInfo[ib].numports = 0;
	boardInfo[ib].int_mask = 0;
    }

    int portcounter = 0;

    for (ib = 0; ib < numboards; ib++) {
	int boardirq = 0;

	retval = -EBUSY;
	addr = SYSTEM_ISA_IOPORT_BASE + ioport[ib];
	if (check_region(addr, 8)) {
	    rtl_printf("dsm_serial: ioports at 0x%x already in use\n",addr);
	    goto err1;
	}
	request_region(addr, 8, "dsm_serial");
	boardInfo[ib].addr = addr;

	retval = -EINVAL;
	switch (boardInfo[ib].type) {
	case BOARD_WIN_COM8:
	    boardInfo[ib].numports = 8;
	    break;
	default:
	    rtl_printf("unknown board type: %d\n",boardInfo[ib].type);
	    goto err1;
	}
#ifdef DEBUG
	rtl_printf("numports=%d\n",boardInfo[ib].numports);
#endif

	retval = -ENOMEM;
	boardInfo[ib].ports = rtl_gpos_malloc(
          boardInfo[ib].numports * sizeof(struct serialPort) );
	if (!boardInfo[ib].ports) goto err1;

	for (ip = 0; ip < boardInfo[ib].numports; ip++) {
	    struct serialPort* port = boardInfo[ib].ports + ip;
	    init_serialPort_struct(port);
	    port->board = boardInfo + ib;
	}

	for (ip = 0; ip < boardInfo[ib].numports; ip++) {
	    struct serialPort* port = boardInfo[ib].ports + ip;

	    port->portNum = portcounter;
	    port->portIndex = ip;
	    port->ioport = ioport0[ib] + (ip * 8);

	    addr = SYSTEM_ISA_IOPORT_BASE + port->ioport;
	    retval = -EBUSY;
	    if (check_region(addr, 8)) {
		rtl_printf("dsm_serial: ioports at 0x%x already in use\n",
			addr);
		goto err1;
	    }
	    request_region(addr, 8, "dsm_serial");
	    port->addr = addr;

	    retval = -ENOMEM;
	    port->xmit.buf = rtl_gpos_malloc( SERIAL_XMIT_SIZE );
	    if (!port->xmit.buf) goto err1;

	    port->sample_queue.buf = rtl_gpos_malloc( SAMPLE_QUEUE_SIZE * sizeof(void*) );
	    if (!port->sample_queue.buf) goto err1;

	    for (i = 0; i < SAMPLE_QUEUE_SIZE; i++) {
	      struct dsm_sample* samp = (struct dsm_sample*)
	      	rtl_gpos_malloc( SIZEOF_DSM_SAMPLE_HEADER +
                                 MAX_DSM_SERIAL_MESSAGE_SIZE );
	      if (!samp) goto err1;
	      port->sample_queue.buf[i] = samp;
	    }
	    port->sample = port->sample_queue.buf[port->sample_queue.head];

	    port->irq = irqs[ib];

	    if (boardirq == 0) boardirq = port->irq;
	    retval = -EINVAL;
	    if (boardirq != port->irq) {
	        rtl_printf("current version only supports one IRQ per board\n");
		goto err1;
	    }

	    /* There are the beginnings of support here for multiple
	     * IRQs per board, but we don't actually support
	     * that yet.
	     */
	    if (numirqs == numboards) port->irq = irqs[ib];
	    else port->irq = irqs[portcounter];

	    if (boardirq == 0) boardirq = port->irq;
	    retval = -EINVAL;
	    if (boardirq != port->irq) {
	        rtl_printf("current version only supports one IRQ per board\n");
		goto err1;
	    }

	    switch (boardInfo[ib].type) {
	    case BOARD_WIN_COM8:
#ifdef DEBUG
		/* read ioport address and irqs for ports on board */
		serial_out(boardInfo + ib,COM8_BC_IR,ip);
		unsigned char x;
		x = serial_in(boardInfo + ib,COM8_BC_BAR);
		rtl_printf("addr=0x%x, enabled=0x%x\n",
		    (x & 0x7f) << 3,(x & 0x80));
		rtl_printf("irq=%d\n",
		    serial_in(boardInfo + ib,COM8_BC_IAR) & 0xf);
#endif
		
		/* configure ioport address and irqs for ports on board */
		serial_out(boardInfo + ib,COM8_BC_IR,ip);
		/* must enable uart for it to respond at this address */
		serial_out(boardInfo + ib,COM8_BC_BAR,(port->ioport >> 3) +
		    COM8_BC_UART_ENABLE);
		serial_out(boardInfo + ib,COM8_BC_IAR,port->irq);

#ifdef DEBUG
		/* read ioport address and irqs for ports on board */
		serial_out(boardInfo + ib,COM8_BC_IR,ip);
		x = serial_in(boardInfo + ib,COM8_BC_BAR);
		rtl_printf("addr=0x%x, enabled=0x%x\n",
		    (x & 0x7f) << 3,(x & 0x80));
		rtl_printf("irq=%d\n",
		    serial_in(boardInfo + ib,COM8_BC_IAR) & 0xf);
#endif
		break;
	    }

	    retval = -ENOMEM;
	    sprintf(devname, "/dev/%s%d", devprefix,portcounter);
	    port->devname = (char *) rtl_gpos_malloc( strlen(devname) + 1 );
	    if (!port->devname) goto err1;
	    strcpy(port->devname,devname);

	    if ( rtl_register_dev(devname, &rtl_dsm_ser_fops,(unsigned long)port) ) {
		printk("Unable to install %s driver\n",devname);
		/* if port->devname is non-zero then it has been registered */
		rtl_gpos_free(port->devname);
		port->devname = 0;
		retval = -EIO;
		goto err1;
	    }


	    portcounter++;
	}

	switch (boardInfo[ib].type) {
	case BOARD_WIN_COM8:
	    if ((retval = rtl_request_isa_irq(boardirq,dsm_serial_irq_handler,
		boardInfo + ib)) < 0) goto err1;
	    break;
	}
	boardInfo[ib].irq = boardirq;
    }

    return 0;

err1:
    if (boardInfo) {
	for (ib = 0; ib < numboards; ib++) {
	    if (boardInfo[ib].irq) rtl_free_isa_irq(boardInfo[ib].irq);
	    boardInfo[ib].irq = 0;

	    if (boardInfo[ib].addr) release_region(boardInfo[ib].addr, 8);
	    boardInfo[ib].addr = 0;

	    if (boardInfo[ib].ports) {
		for (ip = 0; ip < boardInfo[ib].numports; ip++) {
		    struct serialPort* port = boardInfo[ib].ports + ip;

		    if (port->addr) release_region(port->addr, 8);
		    port->addr = 0;
		    if (port->xmit.buf) rtl_gpos_free(port->xmit.buf);
		    port->xmit.buf = 0;

		    if (port->sample_queue.buf) {
			for (i = 0; i < SAMPLE_QUEUE_SIZE; i++)
			  if (port->sample_queue.buf[i])
			      rtl_gpos_free(port->sample_queue.buf[i]);
			rtl_gpos_free(port->sample_queue.buf);
			port->sample_queue.buf = 0;
		    }

		    rtl_sem_destroy(&port->sample_sem);

		    if (port->devname) {
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
    }
err0:
    return retval;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
    rtl_printf("cleanup module: %s\n",devprefix);
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

		if (port->sample_queue.buf) {
		    for (i = 0; i < SAMPLE_QUEUE_SIZE; i++)
		      if (port->sample_queue.buf[i])
			  rtl_gpos_free(port->sample_queue.buf[i]);
		    rtl_gpos_free(port->sample_queue.buf);
		    port->sample_queue.buf = 0;
		}

		rtl_sem_destroy(&port->sample_sem);

		if (port->devname) {
		    rtl_printf("rtl_unregister_dev: %s\n",port->devname);
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
}
