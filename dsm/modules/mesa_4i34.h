#ifndef _MESA_4I34_H
#define _MESA_4I34_H

#undef PDEBUG             /* undef it, just in case */
#ifdef MESA_4I34_DEBUG
#  ifdef __RTCORE_KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "mesa_4i34: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#  ifdef __RTCORE_KERNEL__

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#ifndef MESA_4I34_MAJOR
/* look in Documentation/devices.txt 
 * 60-63,120-127,240-254 LOCAL/EXPERIMENTAL USE
 * 231-239 unassigned
 * 208,209: user space serial ports - what are these?
 */
#define MESA_4I34_MAJOR 0   /* dynamic major by default */
#endif

#define MESA_4I34_MAX_NR_DEVS 4	/* maximum number of Mesa 4I34 cards in sys */
#define MESA_4I34_IO_REGION_SIZE 2 /* number of 1-byte registers */

/* registers on the Mesa 4I34 starting at the ioport address */
#define R_4I34DATA    0 /* 4I34 data register. */
#define R_4I34CONTROL 1 /* 4I34 control register. */
#define R_4I34STATUS  1 /* 4I34 status register. */

/* Masks for R_4I34CONTROL.
*/
#define B_4I34CFGCS           0 /* Chip select. Enables read/write access to the FPGA via R_4I34DATA. */
#define M_4I34CFGCSON         (0x0 << B_4I34CFGCS) /* Enable access. */
#define M_4I34CFGCSOFF        (0x1 << B_4I34CFGCS) /* Disable access. */

#define B_4I34CFGINIT         1 /* Control programming. */
#define M_4I34CFGINITASSERT   (0x0 << B_4I34CFGINIT) /* Wipe current configuration, prepare for programming. */
#define M_4I34CFGINITDEASSERT (0x1 << B_4I34CFGINIT) /* Wait for programming data. */

#define B_4I34CFGWRITE        2 /* Data direction control for R_4I34DATA. */
#define M_4I34CFGWRITEENABLE  (0x0 << B_4I34CFGWRITE) /* CPU --> R_4I34DATA. */
#define M_4I34CFGWRITEDISABLE (0x1 << B_4I34CFGWRITE) /* R_4I34DATA --> CPU. */

#define B_4I34LED             3 /* Red LED control. */
#define M_4I34LEDON           (0x1 << B_4I34LED)
#define M_4I34LEDOFF          (0x0 << B_4I34LED)

/* Masks for R_4I34STATUS.  */
#define B_4I34PROGDUN 0 /* Programming-done flag. Set when the FPGA "program" has been successfully uploaded to the 4I34. */
#define M_4I34PROGDUN (0x1 << B_4I34PROGDUN)

/* How many times to check for PROGDUN bit after programming */
#define MESA_4I34_WAIT_FOR_DUN 200

/* How many jiffies to wait between checks for PROGDUN */
#define MESA_4I34_WAIT_JIFFY 5

/* How many times to double check that PROGDUN stays set.
   Testing is needed to see if this is necessary under linux.
   Set it to 0 to disable paranoid double checking. */
#define MESA_4I34_DOUBLE_CHECK_FOR_DUN 10

/* Number of configuration completion clocks to send after programming.
  (6 should be enough) */
#define MESA_4I34_NUM_CONFIG_COMP_CLOCKS 6

/* size of internel driver buffer, used in writes to the 4i34 */
#define MESA_4I34_BUFFER_SIZE 256
                                                                                
#include <linux/devfs_fs_kernel.h>

extern devfs_handle_t mesa_4i34_devfs_dir;

#  endif /* __RTCORE_KERNEL__ */

#  ifdef __RTCORE_KERNEL__
typedef struct mesa_4i34_device {
  unsigned long ioport;	/* base virtual io address of the mesa_4i34 card */
  devfs_handle_t handle;	/* devfs */
  unsigned char* buf;
  struct semaphore sem;		/* mutual exclusion semaphore */
  struct resource* region;
} mesa_4i34_device;

extern struct file_operations mesa_4i34_fops;

extern int mesa_4i34_major;     /* main.c */
extern int mesa_4i34_nr_devices;     /* main.c */

rtl_ssize_t mesa_4i34_read (struct file *filp, char *buf, rtl_size_t count,
                    loff_t *f_pos);
rtl_ssize_t mesa_4i34_write (struct file *filp, const char *buf, rtl_size_t count,
                     loff_t *f_pos);
loff_t  mesa_4i34_llseek (struct file *filp, loff_t off, int whence);
int     mesa_4i34_ioctl (struct inode *inode, struct file *filp,
                     unsigned int cmd, unsigned long arg);

#  endif /* __RTCORE_KERNEL__ */

/* Look in Documentation/ioctl-number.txt */
#define MESA_4I34_IOC_MAGIC  0xd1
#define MESA_4I34_IOCPROGSTART _IO(MESA_4I34_IOC_MAGIC,   1)
#define MESA_4I34_IOCPROGEND _IO(MESA_4I34_IOC_MAGIC,  2)
#define MESA_4I34_IOC_MAXNR 2

#endif	/* _MESA_4I34_H */

