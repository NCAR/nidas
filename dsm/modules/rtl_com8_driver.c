/*
 * RTLinux serial driver
 *
 * Written by Michael Barabanov
 * Copyright (C) Finite State Machine Labs Inc., 1995-2004
 * All rights reserved.
 *  */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <rtl_com8.h>
#include <linux/ioport.h>
#include <rtl_posixio.h>

static int fifo_fd[RTL_MAX_SERIAL][TOG];
static pthread_t thread;
static int fd[RTL_MAX_SERIAL];
static struct serial_buffer serial_buffers[RTL_MAX_SERIAL];
static struct rtl_serial_struct pcserial[RTL_MAX_SERIAL];

extern int ptog;

void cleanup_module (void);

/* Function below: returns pointer to struct */
struct rtl_serial_struct *rtl_get_serial(int fd)
{
  return &pcserial[rtl_inodes[rtl_fds[fd].devs_index].priv];
}

int rtl_serial_get(struct rtl_serial_struct *serial, void *buf, ssize_t nbytes)
{
  int ret;

  SERIAL_OK_ELSE_RETURN(serial);

  ret = rtl_cb_get (&serial->buffer, buf, nbytes);
  if (!serial->port_blk.irq_num) { //poll to fill 
    int lsr;
    while ((lsr = in_byte (serial, LSR)) & 0x1 && ret < nbytes) {
      unsigned char c = in_byte (serial, RHR);
      rtl_cb_put (&serial->buffer, &c, 1);
      ret++;
    }
  }
	return ret;
}

int rtl_serial_put (struct rtl_serial_struct *serial, 
                    const unsigned char *buf, ssize_t nbytes)
{
  int ret, i;

  SERIAL_OK_ELSE_RETURN(serial);

  if (nbytes > rtl_cb_remaining(&serial->out_buf))
    ret = rtl_cb_remaining(&serial->out_buf);
  else 
    ret = nbytes;
  rtl_cb_put(&serial->out_buf,(void*)buf,ret);
  /* If there was no data pending before, there's no interrupt coming
   and looking for more data. Else, the data we just put in will get
   spun out with the rest coming through the ISR. */
  if (rtl_cb_curdepth(&serial->out_buf) == nbytes && 
     (in_byte(serial, LSR) & 0x20)) {
    unsigned char byte;
    i = rtl_cb_get(&serial->out_buf,&byte,1);
    if (i)
      out_byte(byte,serial,THR);
  }
  return ret;
}

struct serial_buffer {
	char in_buf[RTL_SERIAL_BUFSIZE];
	char out_buf[RTL_SERIAL_BUFSIZE];
};

void rtl_com8_handle(int com_port, struct rtl_serial_struct *serial)
{
  int stat, x;
  unsigned int rx_char_cnt;
  char temp[128];

  while(1)
  {
    stat = inb_p(serial->port_blk.uart_base + 2); //ISR register
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
*/
//      case 2:      // Receive Data Ready

        rx_char_cnt = 0;
        while(inb_p(serial->port_blk.uart_base + 5) & 0x01) 
        // Read fifo until empty
        {
          temp[rx_char_cnt] = inb_p(serial->port_blk.uart_base); // Read data
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
unsigned int rtl_serial_irq(unsigned int irq, struct rtl_frame *frame)
{
  struct rtl_serial_struct *serial = pcserial;
  unsigned char pending_irqs;
  char *int_ptr;
  int i;

  if (irq != COM8_IRQ) {
    rtl_printf("Unknown iir: %x\n",irq);
    return -1;
  }
  int_ptr = (char*)irq_reg;
  pending_irqs = inb_p(int_ptr);   //bit 0-7 = port 0-7 interrupt pending
  pending_irqs &= irq_masks;
  while (pending_irqs) {
    for (i = 0; i < 8; i++) {
      serial = &pcserial[i];
      if(pending_irqs & (1 << i)) {
       rtl_spin_lock (&serial->lock);
       rtl_com8_handle(i, serial);
      }
      rtl_spin_unlock (&serial->lock);
//      serial++;
    }
    pending_irqs = inb_p(int_ptr);
    pending_irqs &= irq_masks;
  }
  rtl_hard_enable_irq(irq);
  return 0;
}


int tcgetattr_com(int *fd, int port, struct Port_blk *term)
{
  *term = rtl_get_serial((int)fd[port])->port_blk;
  return 0;
}

int tcsetattr_com(int *fd)
{
  struct rtl_serial_struct *serial;
  struct Port_blk *term;
  rtl_irqstate_t flags;
  unsigned baud_divisor;
  int temp, i;
  char *base_port;

  serial = rtl_get_serial((int)*fd);
  term = &serial->port_blk;
  request_region(term->uart_base, 72, "rtl_serial");
  for (i = 0; i< RTL_MAX_SERIAL; i++) {
    rtl_spin_lock_irqsave (&serial->lock, flags);

    if (baud_rate[i] != 0) {

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
    else { 
      continue;
    }
    serial++;
    term = &serial->port_blk;
    rtl_spin_unlock_irqrestore(&serial->lock, flags);
  } 

  rtl_request_irq(term->irq_num, rtl_serial_irq);
  return 0;
}

int tcdrain(unsigned int *fd) 
{
  struct rtl_serial_struct *serial = rtl_get_serial(*fd);
  rtl_spin_lock(&serial->lock); 
  while (!(in_byte(serial, LSR) & 0x40)) { }
  rtl_spin_unlock(&serial->lock);
  return 0;
}

static int rtl_serial_open(struct rtl_file *filp) 
{
  if (!(filp->f_flags & O_NONBLOCK)) {
    return -RTL_EACCES;
  }
  filp->f_priv = rtl_inodes[filp->devs_index].priv;
  return 0;
}

static int rtl_serial_release(struct rtl_file *filp) 
{
  int dev = filp->f_priv;
  if (dev > RTL_MAX_SERIAL || pcserial[dev].port_blk.uart_base == 0)
    return -RTL_ENODEV;
  if (pcserial[dev].port_blk.requested_region) {
    release_region(pcserial[dev].port_blk.uart_base, 8);
  }
  return 0;
}

static ssize_t rtl_serial_write(struct rtl_file *filp, const char *buf, size_t count, off_t *pos)
{
  int dev = filp->f_priv;
  if (dev > RTL_MAX_SERIAL || pcserial[dev].port_blk.uart_base == 0)
    return -RTL_ENODEV;
  return rtl_serial_put(&pcserial[dev],buf,count);
}

static ssize_t rtl_serial_read(struct rtl_file *filp, char *buf, size_t count, off_t *pos) 
{
  int dev = filp->f_priv;
  if (dev > RTL_MAX_SERIAL || pcserial[dev].port_blk.uart_base == 0)
    return -RTL_ENODEV;
  return rtl_serial_get(&pcserial[dev],buf,count);
}

static struct rtl_file_operations rtl_serial_fops = {
read:           rtl_serial_read,
write:          rtl_serial_write,
open:           rtl_serial_open,
release:        rtl_serial_release
};

int init_module(void)
{
  char buf_fd[10];
  int devnum, tog;
  struct rtl_serial_struct *serial = NULL;
  char buf_fifo[10];
  char *uart_base;
  int i, base_address;
  struct Port_blk *term;

//irq_reg: UART int. pending reg., bit 0-7 = port 0-7, read-only
  base_address = com8_base;
  irq_reg = base_address + 3;
  for (i=0; i < RTL_MAX_SERIAL; i++) {
    sprintf(buf_fd,"/dev/ttyS%d",i);
    fd[i] = open(buf_fd, O_NONBLOCK);
    if (fd[i] < 0) {
      printf("Unable to open serial device no. %d\n",i);
      cleanup_module();
      return -1;
    }
    for (tog=0; tog<2; tog++) {
      sprintf(buf_fifo,"/dev/rtl_com8_data_%d_%d",i,tog);
      mkfifo(buf_fifo,0777);
      fifo_fd[i][tog] = open(buf_fifo, O_NONBLOCK|O_WRONLY);
      if (fifo_fd[i][tog] == -1) {
        printf("Error, fifo_fd[%d][tog] = -1 (%d)\n",i,errno);
        cleanup_module();
        return -1;
      }
    }
    serial = rtl_get_serial((int)*fd);
    term = &serial->port_blk;
    term->irq_num = COM8_IRQ;
    term->rx_fifo_count = 64;
    term->tx_fifo_count = 1;
    term->rx_in_ptr = 0;
    term->rx_out_ptr = 0;
    term->tx_head = 0;
    term->tx_tail = 0;
    term->uart_base = base_address + (8 * (i + 1));
    uart_base = (char*)term->uart_base;
    *uart_base++ = i;                               //Uart Index
    *uart_base = (term->uart_base >> 3); //Uart base address
    if (baud_rate[i]) {
      *uart_base++ |= 0x80;             //Enable Uart
      *uart_base++ = term->irq_num & 0x0f; //Uart IRQ No.
      term->baud_rate = baud_rate[i];
      irq_masks |= (1 << i);
    }
    else  {
      *uart_base++ |= 0x7f;             //Disable Uart
      *uart_base++ = 0;                 //Uart IRQ No.
      term->baud_rate = 0;
    }
    tcgetattr_com (&fd[i],i,term);
  }
  tcsetattr_com (&fd[0]);

//  pthread_create (&thread, NULL, start_routine, 0);


  for (i = 0; i < RTL_MAX_SERIAL; i++) {

    struct rtl_serial_struct *serial = &pcserial[i];
    rtl_cb_init (&serial->buffer, serial_buffers[i].in_buf, 
      RTL_SERIAL_BUFSIZE);
    rtl_cb_init (&serial->out_buf, serial_buffers[i].out_buf, 
      RTL_SERIAL_BUFSIZE);
    rtl_spin_lock_init (&serial->lock);

    sprintf(buf_fd,"/dev/ttyS%d",i);
    if (rtl_register_dev(buf_fd,&rtl_serial_fops,0)) {
      printk("Unable to register serial device %d\n",i);
      return -i;
    }

    /* save the device number in the priv field */
    devnum = rtl_namei(buf_fd);
    if (devnum == -RTL_ENODEV) {
      printk("Unable to look device up on dev %s\n", buf_fd);
    }
    rtl_inodes[devnum].priv = i;
    decr_dev_usage(devnum); /* counter the incr of rtl_namei() */
  }
  return 0;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char buf_fd[30];
  char buf_fifo[30];
  int tog, i;
  struct rtl_serial_struct *serial = NULL;

//  pthread_cancel(thread);
//  pthread_join(thread,NULL);
  for (i=0; i < RTL_MAX_SERIAL; i++){
    close(fd[i]);
    sprintf(buf_fd,"/dev/ttyS%d",i);
    rtl_unregister_dev(buf_fd);
    serial = &pcserial[i];
    if (serial->port_blk.irq_num)
      rtl_free_irq(serial->port_blk.irq_num);
    for (tog=0; tog<2; tog++) {
      sprintf(buf_fifo,"/dev/rtl_com8_data_%d_%d",i,tog);
      close(fifo_fd[i][tog]);
      unlink(buf_fifo);
    }
  }
}
