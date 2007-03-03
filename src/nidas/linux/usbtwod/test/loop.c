/* loop.c
 * Cheezy program to test out USB loopback gadget.
 */
#include <fcntl.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char buffer[5000];

main()
{
  int n, i, len;
  fd_set  my_set;
  struct timeval t_out;
  int fd = open("/dev/usbtwod0", O_RDWR);

  if (fd < 0)
  {
    printf("open failure\n");
    exit(1);
  }

  for (i = 0; i < 99; ++i)
  {
    memset(buffer, 0, 5000);
    sprintf(buffer, "Hello, world #%02d", i);
    len = strlen(buffer);
    for (; len < 4096; ++len)
      buffer[len] = '0' + len%10;
    buffer[3500] = 0;
    len = strlen(buffer);

    n = write(fd, buffer, len);
    printf("write: n = %d, len was %d\n", n, len);

    FD_ZERO(&my_set);
    FD_SET(fd, &my_set);
    n = select(fd+1, &my_set, 0, 0, 0);
    printf("select = %d.\n", n);

    memset(buffer, 0, 5000);
    n = read(fd, buffer, len);
    printf("read: n = %d, [%s]\n", n, buffer);
  }

  t_out.tv_sec = 5;
  t_out.tv_usec = 0;

  printf("testing select(2) before writing, this should block for 5 seconds.\n");
  FD_ZERO(&my_set);
  FD_SET(fd, &my_set);
  n = select(fd+1, &my_set, 0, 0, &t_out);
  printf("select = %d.\n", n);

  memset(buffer, 0, 100);
  sprintf(buffer, "Select timeout test", i);
  len = strlen(buffer);

  n = write(fd, buffer, len);
  printf("write: n = %d\n", n);

  memset(buffer, 0, 100);
  n = read(fd, buffer, len);
  printf("read: n = %d, [%s]\n", n, buffer);

  close(fd);
}
