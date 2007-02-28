/* Simple program to test usb gadget loopback.  
 */
#include <sys/select.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char buffer[5000];

main()
{
  int n, i;
  fd_set  my_set;
  FILE * fp = fopen("/dev/usbtwod0", "r+");

  if (fp == NULL)
  {
    printf("open failure\n");
    exit(1);
  }

  int fd = fileno(fp);

  for (i = 0; i < 25; ++i)
  {
    memset(buffer, 0, 100);
    sprintf(buffer, "Hello, world #%02d", i);
    n = fwrite(buffer, 16, 1, fp);
    printf("write: n = %d\n", n);

    n = fread(buffer, 16, 1, fp);
    printf("read: n = %d, [%s]\n", n, buffer);
  }
}
