/* com8.h

   Time-stamp: <Thu 26-Aug-2004 06:46:57 pm>

   RTLinux serial driver for the ISA bus based Com8 card.

   The 'usr/ck???.cc' application is a test program that
   can be used to exercise this modules functionality.

   Original Author: Michael Barabanov
   Copyright (C) Finite State Machine Labs Inc., 1995-2004

   Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research 2004

   Revisions:

*/

#ifndef __RTL_SERIAL_H__
#define __RTL_SERIAL_H__

#include <rtl_conf.h>
#include <rtl_buffer.h>

#define COM8_BASE        0xf7000300
#define BUFFER_SIZE      1024
#define COM8_IRQ         3

struct Port_blk
{
  unsigned int uart_base;
  unsigned int irq_num;
  int baud_rate;
  int rx_in_ptr;
  int rx_out_ptr;
  int rx_fifo_count;
  int tx_head;
  int tx_tail;
  int tx_fifo_count;
  int tx_fifo_max;
  int requested_region;
};
typedef struct Port_blk Port_blk;

// int com8_check(int com_port);
// int com8_getch(int com_port);
// void com8_gets(int com_port, char *str);
//int rtl_serial_get(struct rtl_serial_struct *serial, void *buf, ssize_t nbytes);
//int rtl_serial_put(struct rtl_serial_struct *serial, const unsigned char *buf,
//                        ssize_t nbytes);
struct rtl_serial_struct *rtl_get_serial(int fd);
int tcgetattr_com(int *fd, int port,struct Port_blk *term);
int tcsetattr_com(void);
int tcdrain(int fd);

int com8_base;
int init_id;
char com8_tx_buf[8][BUFFER_SIZE];
char com8_rx_buf[8][BUFFER_SIZE];

int   irq_masks;

// void com8_handle(int port_num);
// void com8_putch(int com_port, char c);
// void com8_puts(int com_port, char *str);
// int com8_read(int com_port);
// void com8_flush(int port_num);

Port_blk com8_blk[8];
Port_blk port_blk;

struct rtl_serial_struct
{
  struct Port_blk port_blk;
  rtl_cb_t buffer;
  rtl_cb_t out_buf;
  rtl_spinlock_t lock;
};

#endif
