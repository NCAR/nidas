/* rtl_com8_driver.c

   Time-stamp: <2004-08-19 14:17:45 spowart>

   RTLinux serial driver for the ISA bus based Com8 card.

   The 'usr/ck???.cc' application is a test program that
   can be used to exercise this modules functionality.

   Original Author: Michael Barabanov
   Copyright (C) Finite State Machine Labs Inc., 1995-2004

   Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research 2004

   Revisions:

*/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <rtl_posixio.h>
#include <linux/ioport.h>

#include <rtl_com8.h>
#include <init_modules.h>

#define RTL_SERIAL_BUFSIZE      2048
#define RTL_MAX_SERIAL          8

#define IER 1
#define DLM 1
#define FCR 2
#define RBR 0
#define THR 0
#define LCR 3
#define MCR 4
#define LSR 5
#define MSR 6
#define TOG 2

#define SERIAL_OK_ELSE_RETURN(serial) \
        do { if (!serial->port_blk.uart_base) return -RTL_EINVAL; } while(0)

#define out_byte(byte, serial, offset) \
        outb_p((byte), (serial)->port_blk.uart_base + (offset))
#define in_byte(serial, offset) \
        inb_p((serial)->port_blk.uart_base + (offset))

static int fifo_fd[RTL_MAX_SERIAL][TOG];
static int fd[RTL_MAX_SERIAL];
static int irq_reg;

extern int module_loading;
extern int ptog;     // this varible is toggled every second
extern struct serialTable serialCfg[];

struct serial_buffer {
  char in_buf[RTL_SERIAL_BUFSIZE];
  char out_buf[RTL_SERIAL_BUFSIZE];
};
static struct serial_buffer serial_buffers[RTL_MAX_SERIAL];
static struct rtl_serial_struct pcserial[RTL_MAX_SERIAL];

void cleanup_module (void);

void rtl_com8_handle(int com_port, struct rtl_serial_struct *serial)
{
  int stat;
  unsigned int rx_char_cnt;
  char temp[128];

  while(1)
  {
    stat = inb_p((serial)->port_blk.uart_base + 2); //ISR register
    if(stat  & 1)                  //No interrupt pending
      break;
/*
    stat = (stat >> 1) & 3;
    switch(stat)
    {
      case 0:     // Modem Status Interrupt
        x = inb_p(serial->port_blk.uart_base + 6);
        break;

      case 1:     // TX Holding Register Empty
        if(serial->port_blk.tx_tail == serial->port_blk.tx_head)
        {

        }
        else
        {
          addr = (ptr->uart_base + 5);
          if((inb_p(serial->port_blk.uart_base + 5) & 0x20) &&
             (serial->port_blk.tx_tail != serial->port_blk.tx_head))
          {
            for(x=0; x < serial->port_blk.tx_fifo_max; x++)
            {
              addr = ptr->uart_base;
              outb_p(com_tx_buf[com_port][serial->port_blk.tx_head++],
                      serial->port_blk.uart_base);
              if(serial->port_blk.tx_head == BUFFER_SIZE)
                serial->port_blk.tx_head =0;

              if(serial->port_blk.tx_head == serial->port_blk.tx_tail)
                break;
            }
          }
        }
        break;

      case 2:      // Receive Data Ready
*/
        rx_char_cnt = 0;
        while(inb_p(serial->port_blk.uart_base + 5) & 0x01)
        {
          // Read fifo until empty
          temp[rx_char_cnt] = inb_p(serial->port_blk.uart_base); // Read data
          rtl_printf("(%s) %s: &temp[%d] = %d\n",
                     __FILE__, __FUNCTION__, rx_char_cnt, temp[rx_char_cnt]);
          serial->port_blk.rx_in_ptr++;
          rx_char_cnt++;
          if(serial->port_blk.rx_in_ptr == BUFFER_SIZE)
        serial->port_blk.rx_in_ptr = 0;
        }
        temp[rx_char_cnt] = '\r'; //Stuff char.
        rx_char_cnt++;
        write(fifo_fd[com_port][ptog],&temp,rx_char_cnt);
        break;
/*
      case 3:      // Line status interrupt
        x = inb_p(serial->port_blk.uart_base + 5);
        break;
    }
*/
  }
}

int rtl_serial_get (struct rtl_serial_struct *serial, void *buf, ssize_t nbytes)
{
  rtl_irqstate_t flags;
  int ret;

  SERIAL_OK_ELSE_RETURN(serial);

  rtl_spin_lock_irqsave (&serial->lock, flags);
  ret = rtl_cb_get (&serial->buffer, buf, nbytes); //as many as possible
  if (!serial->port_blk.irq_num)                   //poll to fill
  {
    int lsr;
    while ((lsr = in_byte (serial, LSR)) & 0x1 && ret < nbytes)
    {
      unsigned char c = in_byte (serial, RBR);
      rtl_cb_put (&serial->buffer, &c, 1);
      ret++;
    }
  }
  rtl_spin_unlock_irqrestore (&serial->lock, flags);
  return ret;
}

int rtl_serial_put (struct rtl_serial_struct *serial, const unsigned char *buf, ssize_t nbytes)
{
  rtl_irqstate_t flags;
  int i = 0;

  SERIAL_OK_ELSE_RETURN(serial);

  rtl_spin_lock_irqsave (&serial->lock, flags);
  while (i < nbytes)
  {
    while (!(in_byte(serial, LSR) & 0x20)) { }
    out_byte(buf[i], serial, THR);
    i++;
  }
  rtl_spin_unlock_irqrestore (&serial->lock, flags);
  return 0;
}

unsigned int rtl_serial_irq(unsigned int irq, struct rtl_frame *frame)
{
  struct rtl_serial_struct *serial;
  unsigned char pending_irqs;
  char *int_ptr;
  int i;

  if (irq != VIPER_CPLD_IRQ)
  {
    rtl_printf("(%s) %s: Unknown irq: %x\n", __FILE__, __FUNCTION__, irq);
    cleanup_module();
    return -1;
  }
  int_ptr = (char*)irq_reg;
  pending_irqs = inb_p(int_ptr);   //bit 0-7 = port 0-7 interrupt pending
  pending_irqs &= irq_masks;

  while (pending_irqs)
  {
    for (i = 0; i < 3; i++) // was RTL_MAX_SERIAL
    {
      serial = rtl_get_serial((int)fd[i]);

      if(pending_irqs & (1 << i))
      {
        rtl_spin_lock (&serial->lock);
        rtl_com8_handle(i, serial);
      }
      rtl_spin_unlock (&serial->lock);
    }
    pending_irqs = inb_p(int_ptr);
    pending_irqs &= irq_masks;
  }
  rtl_hard_enable_irq(irq);
  return 0;
}

struct rtl_serial_struct *rtl_get_serial(int fd)
{
  return &pcserial[rtl_inodes[rtl_fds[fd].devs_index].priv];
}

int tcgetattr_com(int *fd, int port, struct Port_blk *term)
{
  *term = rtl_get_serial((int)fd[port])->port_blk;
  return 0;
}

int tcsetattr_com (void)
{
  struct rtl_serial_struct *serial;
  struct Port_blk *term;
  rtl_irqstate_t flags;
  unsigned baud_divisor;
  int temp, i;
  char *base_port;

  serial = rtl_get_serial((int)fd[0]);
  term = &serial->port_blk;

  request_region(COM8_BASE, 72, "rtl_serial");
  for (i = 0; i < RTL_MAX_SERIAL; i++)
  {
    rtl_spin_lock_irqsave (&serial->lock, flags);

    if (serialCfg[i].baud_rate != 0)
    {
      /* Set up the baud rate */
      baud_divisor = (unsigned) (1843200L / (term->baud_rate * 16L));
      base_port = (char*)term->uart_base;

      temp = in_byte(serial, LCR);
      out_byte(temp | 0x80, serial,LCR); //Switch to DLM register
      out_byte(baud_divisor >> 8, serial, DLM);
      out_byte(baud_divisor & 0xff, serial, 0);
      temp = in_byte(serial, LCR);
      out_byte(temp & 0x7f, serial,LCR); //Switch to IER register
      out_byte(3, serial, LCR);      /* 8-bits, 1 stop, no parity */
      out_byte(0x0b, serial, MCR);   /* Set RTS, DTR, OUT2 */

      temp = in_byte(serial,0);
      temp = in_byte(serial,0);
      temp = in_byte(serial,0);
      temp = in_byte(serial,0);

      /* Enable FIFO's if present */

      out_byte(0, serial, FCR);        /* Reset transmit and recieve FIFOs */
      out_byte(1, serial, FCR);        /* Reset transmit and recieve FIFOs */

      temp = in_byte(serial, LCR);     /* Get current value */
      out_byte(0xbf, serial, LCR);     /* Turns on Enhanced registers */
      out_byte(0x10, serial, FCR);     /* Enable enhanced bits */
      out_byte(0x30, serial, IER);     /* Select RX FIFO table 'D' */
      out_byte(term->rx_fifo_count, serial, 0);   //IRQ trigger level
      out_byte(0xb0, serial, IER);     /* Select TX FIFO table 'D' */
      out_byte(term->tx_fifo_count, serial, 0);    //IRQ trigger level

      if(term->tx_fifo_count > 0)
        term->tx_fifo_max = 128 - term->tx_fifo_count;
      else
        term->tx_fifo_max = 0;

      out_byte(temp, serial, LCR);   /* Restore original value */
      out_byte(0xa1, serial, FCR);   // DMA Mode 0 (IRQ for each char.
      out_byte(0xa7, serial, FCR);   /* Reset all FIFO counters to 0 */
//      out_byte(0xa9, serial, FCR);   // DMA Mode 1 (IRQ on Trigger Lvl.
//      out_byte(0xaf, serial, FCR);   /* Reset all FIFO counters to 0 */

      /* End of FIFO CODE */
      out_byte(1, serial, IER);     /* Enable RX interrupts only!*/
    }
    else
      continue;

    serial++;
    term = &serial->port_blk;
    rtl_spin_unlock_irqrestore(&serial->lock, flags);
  }
  return 0;
}

int tcdrain(int fd)
{
  struct rtl_serial_struct *serial = rtl_get_serial(fd);
  rtl_spin_lock(&serial->lock);

  while (!(in_byte(serial, LSR) & 0x40)) { }

  rtl_spin_unlock(&serial->lock);
  return 0;
}

static int rtl_serial_open(struct rtl_file *filp)
{
  rtl_printf("(%s) %s: called\n", __FILE__, __FUNCTION__);

  if (!(filp->f_flags & O_NONBLOCK))
    return -RTL_EACCES;

  filp->f_priv = rtl_inodes[filp->devs_index].priv;
  return 0;
}

static ssize_t rtl_serial_write(struct rtl_file *filp, const char *buf, size_t count, off_t *pos)
{
  rtl_printf("(%s) %s: called\n", __FILE__, __FUNCTION__);
  int dev = filp->f_priv;

  if (dev > RTL_MAX_SERIAL || pcserial[dev].port_blk.uart_base == 0)
    return -RTL_ENODEV;

  rtl_serial_put(&pcserial[dev],buf,count);
  return count;
}

static ssize_t rtl_serial_read(struct rtl_file *filp, char *buf, size_t count, off_t *pos)
{
  rtl_printf("(%s) %s: called\n", __FILE__, __FUNCTION__);
  int dev = filp->f_priv;
  if (dev > RTL_MAX_SERIAL || pcserial[dev].port_blk.uart_base == 0)
    return -RTL_ENODEV;

  return rtl_serial_get(&pcserial[dev],buf,count);
}

static struct rtl_file_operations rtl_serial_fops = {
  read:           rtl_serial_read,
  write:          rtl_serial_write,
  open:           rtl_serial_open
};

int init_module(void)
{
  rtl_printf("(%s) %s: compiled on %s at %s\n",
             __FILE__, __FUNCTION__, __DATE__, __TIME__);

  char devstr[30];
  char *uart_base;
  int temp, devnum, tog, i;
  struct rtl_serial_struct *serial = NULL;
  struct Port_blk *term;

  // create the device driver first and register it...
  for (i = 0; i < RTL_MAX_SERIAL; i++)
  {
    struct rtl_serial_struct *serial = &pcserial[i];
    rtl_cb_init (&serial->buffer, serial_buffers[i].in_buf,
                 RTL_SERIAL_BUFSIZE);
    rtl_cb_init (&serial->out_buf, serial_buffers[i].out_buf,
                 RTL_SERIAL_BUFSIZE);
    rtl_spin_lock_init (&serial->lock);

    sprintf(devstr,"/dev/ttyS%d",i);
    if (rtl_register_dev(devstr,&rtl_serial_fops,0))
    {
      rtl_printf("(%s) %s: Unable to register serial device %d\n",
                 __FILE__, __FUNCTION__, i);
      cleanup_module();
      return -i;
    }

    /* save the device number in the priv field */
    devnum = rtl_namei(devstr);
    if (devnum == -RTL_ENODEV)
      rtl_printf("(%s) %s: Unable to look up '%s'\n",
                 __FILE__, __FUNCTION__, devstr);

    rtl_inodes[devnum].priv = i;
    decr_dev_usage(devnum); /* counter the incr of rtl_namei() */
  }

  // use the driver as an application...
  //irq_reg: UART int. pending reg., bit 0-7 = port 0-7, read-only
  irq_reg = COM8_BASE + 3;
  irq_masks = 0;
  for (i = 0; i < 3; i++) // was RTL_MAX_SERIAL
  {
    if (i == 0) serialCfg[i].baud_rate = 9600;
    else        serialCfg[i].baud_rate = 0;

    sprintf(devstr,"/dev/ttyS%d",i+5);
    fd[i] = open(devstr, O_NONBLOCK);

    rtl_printf("(%s) %s: fd[%d] = 0x%lx\n", __FILE__, __FUNCTION__, i, fd[i]);
    if (fd[i] < 0)
    {
      rtl_printf("(%s) %s: Unable to open serial device '%s'\n",
                 __FILE__, __FUNCTION__, devstr);
      cleanup_module();
      return -1;
    }
    serialCfg[i].fptr = fd[i];
    for (tog=0; tog<2; tog++)
    {
      sprintf(devstr,"/dev/rtl_com8_data_%d_%d",i,tog);
      mkfifo(devstr,0777);
      fifo_fd[i][tog] = open(devstr, O_NONBLOCK|O_WRONLY);

      if (fifo_fd[i][tog] == -1)
      {
        rtl_printf("(%s) %s: Error, fifo_fd[%d][%d] = -1 (%d)\n",
                   __FILE__, __FUNCTION__, i, tog, errno);
        cleanup_module();
        return -1;
      }
    }
    serial = rtl_get_serial(fd[i]);
    term = &serial->port_blk;
    term->irq_num = COM8_IRQ;
    term->rx_fifo_count = 64;
    term->tx_fifo_count = 1;
    term->rx_in_ptr = 0;
    term->rx_out_ptr = 0;
    term->tx_head = 0;
    term->tx_tail = 0;
    term->uart_base = COM8_BASE + (8 * (i + 1));
    uart_base = (char*)term->uart_base;

    outb(i, uart_base++);                               //Uart Index
    outb( (term->uart_base >> 3) & 0x0fff, uart_base ); //Uart base address

    if (serialCfg[i].baud_rate)
    {
      temp = inb( uart_base );
      outb( temp | 0x80, uart_base++);          //Enable Uart
      outb( term->irq_num & 0x0f, uart_base++); //Uart IRQ No.

      term->baud_rate = serialCfg[i].baud_rate;
      irq_masks |= (1 << i);
    }
    else
    {
      temp = inb( uart_base );
      outb( temp | 0x7f, uart_base++);  //Disable Uart
      outb( 0,           uart_base++);  //Uart IRQ No.
      term->baud_rate = 0;
      continue;
    }
    tcgetattr_com (&fd[i],i,term);
  }
  tcsetattr_com ();

//  pthread_create (&thread, NULL, start_routine, 0);
  module_loading = 0;
  return 0;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char devstr[30];
  int tog, i;

//  pthread_cancel(thread);
//  pthread_join(thread,NULL);
  for (i = 0; i < RTL_MAX_SERIAL; i++)
  {
    close(fd[i]);
    sprintf(devstr,"/dev/ttyS%d",i);
    rtl_unregister_dev(devstr);

    for (tog=0; tog<2; tog++)
    {
      sprintf(devstr,"/dev/rtl_com8_data_%d_%d",i,tog);
      close(fifo_fd[i][tog]);
      unlink(devstr);
    }
  }
  release_region(COM8_BASE, 72);
}
