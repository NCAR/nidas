/*
 * rtl_com8.h
 *
 *  */

#ifndef __RTL_SERIAL_H__
#define __RTL_SERIAL_H__

#include <rtl_conf.h>
#include <rtl_buffer.h>

#define COM8_BASE        0x30000300
#define BUFFER_SIZE      1024
#define COM8_IRQ         3
#define RTL_SERIAL_BUFSIZE      2048
#define RTL_MAX_SERIAL          8
#define IER 1
#define DLM 1
#define FCR 2
#define RHR 0
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
struct Port_blk {
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

// int rtl_com8_check(int com_port);
// int rtl_com8_getch(int com_port);
// void rtl_com8_gets(int com_port, char *str);
//int rtl_serial_get(struct rtl_serial_struct *serial, void *buf, ssize_t nbytes);
//int rtl_serial_put(struct rtl_serial_struct *serial, const unsigned char *buf,
//                        ssize_t nbytes);
struct rtl_serial_struct *rtl_get_serial(int fd);
int tcgetattr_com(int *fd, int port,struct Port_blk *term);
int tcsetattr_com(int *fd);
int tcdrain(unsigned int *fd);


int baud_rate[8];

int com8_base;
int init_id;
char com8_tx_buf[8][BUFFER_SIZE];
char com8_rx_buf[8][BUFFER_SIZE];

int   irq_masks;
static int   irq_reg;

// void rtl_com8_handle(int port_num);
// void rtl_com8_putch(int com_port, char c);
// void rtl_com8_puts(int com_port, char *str);
// int rtl_com8_read(int com_port);
// void rtl_com8_flush(int port_num);

Port_blk com8_blk[8];
Port_blk port_blk;

struct rtl_serial_struct {
	struct Port_blk port_blk;
	rtl_cb_t buffer;
	rtl_cb_t out_buf;
	rtl_spinlock_t lock;
};

#endif
