/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
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
