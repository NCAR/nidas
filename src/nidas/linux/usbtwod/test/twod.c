/* Test program to read 4k data records from the driver, add P2d_rec header
 * from header.h and then write the data to a disk file, viewable with xpms2d.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

// Resides somewhere in the raf repository.
#include "/home/local/include/header.h"
#include "../usbtwod.h"

main()
{
  int i, n;
  fd_set  my_set;
  int fd;
  FILE * fp_o = fopen("data.2d", "w+");
  P2d_rec rec;
  char buffer[5000];

  unsigned long tas = 125;

  rec.id = htons(0x4331);

  while ((fd = open("/dev/usbtwod0", O_RDWR)) < 0)
  {
    usleep(10000);
//    printf("open failure\n");
//    exit(1);
  }

  printf("/dev/usbtwod opened\n");

  // Test sending tas.
  ioctl(fd, USB2D_SET_TAS, tas);

  n = write(fd, rec.data, 3);
  printf("write n = %d\n", n);

  for (i = 0; i < 1000; )
  {
    n = read(fd, buffer, 4096+8);
//    printf("read n = %d\n", n);

    if (n == 4096+8)
    {
      unsigned long t = ((long *)buffer)[0];
      memcpy(rec.data, &buffer[sizeof(dsm_sample_t)], 4096);


      rec.msec = htons(t % 1000);
      t /= 1000;
      rec.hour = htons(t / 3600);
      t -= (t/3600) * 3600;
      rec.minute = htons(t / 60);
      t -= (t/60) * 60;
      rec.second = htons(t / 60);

      fwrite(&rec, sizeof(rec), 1, fp_o);

      ++i;
//      usleep(10000);
    }
  }

  close(fd);
  fclose(fp_o);
}

