/* Test program to read 4k data records from the driver, add P2d_rec header
 * from header.h and then write the data to a disk file, viewable with xpms2d.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

// Resides somewhere in the raf repository.
#include "/home/local/include/header.h"

main()
{
  int i, n;
  fd_set  my_set;
  int fd;
  FILE * fp_o = fopen("data.2d", "w+");
  P2d_rec rec, tim;

  while ((fd = open("/dev/usbtwod0", O_RDONLY)) < 0)
  {
    usleep(10000);
//    printf("open failure\n");
//    exit(1);
  }

  printf("/dev/usbtwod opened\n");
  rec.id = htons(0x4331);
  tim.hour = 5;
  tim.minute = 17;
  tim.second = 0;
  tim.msec = 0;

  for (i = 0; i < 1000; )
  {
    n = read(fd, rec.data, 4096);
//    printf("read n = %d\n", n);

    if (n == 4096)
    {
      fwrite(&rec, sizeof(rec), 1, fp_o);
      memset(rec.data, 0, 4096);
      tim.msec += 10;
      if (tim.msec >= 1000)
      {
        tim.second++;
        tim.msec = 0;
      }
      ++i;
      usleep(10000);
    }
    rec.hour = htons(tim.hour);
    rec.minute = htons(tim.minute);
    rec.second = htons(tim.second);
    rec.msec = htons(tim.msec);
  }

  close(fd);
  fclose(fp_o);
}

